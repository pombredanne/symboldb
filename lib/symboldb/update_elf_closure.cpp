/*
 * Copyright (C) 2012, 2013 Red Hat, Inc.
 * Written by Florian Weimer <fweimer@redhat.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <symboldb/update_elf_closure.hpp>
#include <cxxll/pgresult_handle.hpp>
#include <cxxll/pgconn_handle.hpp>
#include <cxxll/pg_exception.hpp>
#include <cxxll/pg_query.hpp>
#include <cxxll/pg_response.hpp>
#include <cxxll/string_support.hpp>

#include <map>
#include <set>

#include <cassert>
#include <cstring>

using namespace cxxll;

namespace {
  struct file_ref {
    database::file_id id;
    std::string name;
    std::string package;
    file_ref(int fid, const std::string &file_name, const std::string &pkg)
      : id(fid), name(file_name), package(pkg)
    {
    }

    // Heuristic to rate the match between the two paths.
    int priority(const char *needed_path) const;
  };

  // Returns true if both absolute path names refer to files in the
  // same directory.
  bool
  same_directory(const char *left, const char *right)
  {
    const char *leftslash = strrchr(left, '/');
    const char *rightslash = strrchr(right, '/');
    if (leftslash == nullptr || rightslash == nullptr
	|| (leftslash - left) != (rightslash - right)) {
      return false;
    }
    return memcmp(left, right, leftslash - left) == 0;
  }

  int
  file_ref::priority(const char *needed_path) const
  {
    int prio = 0;
    enum {
      lib_prio = 100000,
      directory_prio = 10000
    };
    // The standard library directories are strongly preferred.
    if (strncmp(name.c_str(), "/lib/", 5) == 0
	|| strncmp(name.c_str(), "/lib64/", 7) == 0
	|| strncmp(name.c_str(), "/usr/lib/", 9) == 0
	|| strncmp(name.c_str(), "/usr/lib64/", 11) == 0) {
	prio += lib_prio;
    }
    if (same_directory(name.c_str(), needed_path)) {
      prio += directory_prio;
    }
    // Prefer libraries in the same file system area, with a shared
    // initial path.
    size_t sz = std::min(name.size(), strlen(needed_path));
    for (unsigned i = 0; i < sz && name.at(i) == needed_path[i]; ++i) {
      prio += 2;
    }
    // Deeply nested libraries are less prefered.
    prio -= name.size();
    return prio;
  }

  typedef std::multimap<std::string, file_ref> soname_map;
  typedef std::map<std::string, soname_map> arch_soname_map;
  typedef std::map<database::file_id, std::set<database::file_id> >
    dependency_map;

  // Find the most suitable library for a particular SONAME reference.
  database::file_id
  lookup(const arch_soname_map &arch_soname,
	 const std::string &arch, const std::string &needed_name,
	 database::file_id needing_file, const char *needing_path,
	 update_elf_closure_conflicts *conflicts, bool debug = false)
  {
    arch_soname_map::const_iterator soname(arch_soname.find(arch));
    if (soname == arch_soname.end()) {
      return database::file_id();
    }
    const std::pair<soname_map::const_iterator,
		    soname_map::const_iterator> providers
      (soname->second.equal_range(needed_name));
    if (providers.first == providers.second) {
      if (conflicts) {
	conflicts->missing(needing_file, needed_name);
      }
      return database::file_id();
    }

    soname_map::const_iterator p(providers.first);
    const file_ref *best = &p->second;
    ++p;

      // There is only one candidate, so it has to be the right one.
    if (p == providers.second) {
      return best->id;
    }

    // Otherwise, we have to compare the priorities of the available
    // candidates.
    int best_priority = best->priority(needing_path);
    if (debug) {
      fprintf(stderr, "info: closure: resolving %s %s\n",
	      needing_path, needed_name.c_str());
      fprintf(stderr, "info: closure:     %s %d\n",
	      best->name.c_str(), best_priority);
    }
    do {
      int prio = p->second.priority(needing_path);
      if (debug) {
	fprintf(stderr, "info: closure:     %s %d\n",
		p->second.name.c_str(), prio);
      }
      // On a file name conflict, pick the package with the
      // lexicographically smaller name.
      if (prio > best_priority
	  || (p->second.name == best->name
	      && p->second.package < best->package)) {
	best = &p->second;
	best_priority = prio;
      }
      ++p;
    } while (p != providers.second);
    if (debug) {
      fprintf(stderr, "info: closure:   winner: %s %d\n",
	      best->name.c_str(), best_priority);
    }
    if (conflicts) {
      std::vector<database::file_id> choices;
      choices.push_back(best->id);
      for (p = providers.first;  p != providers.second; ++p) {
	database::file_id fid = p->second.id;
	if (fid != best->id) {
	  choices.push_back(fid);
	}
      }
      conflicts->conflict(needing_file, needed_name, choices);
    }
    return best->id;
  }

  // Special case for files which should not contribute to conflicts.
  bool
  ignored_file_name(const std::string &path)
  {
    return (starts_with(path, "/lib/")
	    && (starts_with(path, "/lib/i686/nosegneg/")
		|| (starts_with(path, "/lib/rtkaio/")
		    && (starts_with(path, "/lib/rtkaio/librtkaio-")
			|| starts_with(path, "/lib/rtkaio/i686/nosegneg/")))))
      || starts_with(path, "/lib64/rtkaio/librtkaio-");
  }

  // Special case for packages which should not contribute to
  // conflicts.
  bool
  ignored_package_name(const std::string &pkg)
  {
    return pkg == "compat-gcc-34-c++"
      || pkg == "compat-glibc";
  }

  // Suppress some common conflicts, caused by compatibility packages
  // and sub-architecture DSOs.
  void
  ignore_some_conflicts(arch_soname_map &arch)
  {
    std::vector<soname_map::iterator> ignore_queue;

    for (std::pair<const std::string, soname_map> &archp : arch) {
      soname_map &soname(archp.second);
      for (soname_map::iterator p = soname.begin(), pend = soname.end();
	   p != pend; ) {
	soname_map::iterator q = p;
	size_t count = 0;
	ignore_queue.clear();
	while (q != pend && q->first == p->first) {
	  ++count;
	  if (ignored_file_name(q->second.name)
	      || ignored_package_name(q->second.package)) {
	    ignore_queue.push_back(q);
	  }
	  ++q;
	}
	// We must make sure that at least one non-ignored file is
	// left.  In addition, it does not make much sense to remove
	// the spurious conflicts if there are still remaining
	// conflicts, so we only proceed if there is just one
	// remaining resolution.
	if (ignore_queue.size() + 1 == count) {
	  for (soname_map::iterator ignore : ignore_queue) {
	    soname.erase(ignore);
	  }
	}
	p = q;
      }
    }
  }

  std::string synthesize_soname(const std::string &path)
  {
    size_t slash = path.rfind('/');
    if (slash == std::string::npos) {
      return path;
    } else {
      return std::string(path.begin() + slash + 1, path.end());
    }
  }
} // namespace

void
update_elf_closure(pgconn_handle &conn, database::package_set_id id,
		   update_elf_closure_conflicts *conflicts)
{
  assert(conn.transactionStatus() == PQTRANS_INTRANS);
  bool debug = false;

  // Obtain the list of SONAME providers.  There can be multiple DSOs
  // which have the same SONAME, and packages can conflict and install
  // different files at the same path.
  pgresult_handle res;
  pg_query_binary
    (conn, res,
     "SELECT ef.arch::text, COALESCE(ef.soname, ''), file_id, f.name, p.name"
     " FROM symboldb.package_set_member psm"
     " JOIN symboldb.package p USING (package_id)"
     " JOIN symboldb.file f USING (package_id)"
     " JOIN symboldb.elf_file ef USING (contents_id)"
     " WHERE psm.set_id = $1 AND ef.e_type = 3", id.value());
  // ef.e_type == ET_DYN is a restriction to DSOs.

  arch_soname_map arch_soname;
  {
    std::string arch;
    std::string soname;
    int fid;
    std::string file_name;
    std::string pkg;
    for (int row = 0, end = res.ntuples(); row < end; ++row) {
      pg_response(res, row, arch, soname, fid, file_name, pkg);
      if (soname.empty()) {
	soname = synthesize_soname(file_name);
      }
      arch_soname[arch].insert(std::make_pair(soname,
					      file_ref(fid, file_name, pkg)));
    }
  }

  ignore_some_conflicts(arch_soname);

  dependency_map closure;
  pg_query_binary
    (conn, res,
     "SELECT ef.arch::text, en.name, file_id, f.name"
     " FROM symboldb.package_set_member psm"
     " JOIN symboldb.file f USING (package_id)"
     " JOIN symboldb.elf_file ef USING (contents_id)"
     " JOIN symboldb.elf_needed en USING (contents_id)"
     " WHERE psm.set_id = $1", id.value());
  size_t elements = 0;
  {
    std::string arch;
    std::string needed_name;
    int fid;
    std::string needing_path;
    for (int row = 0, end = res.ntuples(); row < end; ++row) {
      pg_response(res, row,
		  arch, needed_name, fid, needing_path);
      database::file_id needing_file(fid);
      database::file_id library =
	lookup(arch_soname, arch, needed_name,
	       needing_file, needing_path.c_str(),
	       conflicts);
      if (library != database::file_id()) {
	elements += closure[needing_file].insert(library).second;
      }
    }
  }

  // Compute the transitive closure.  Iterate until we reach a
  // fixpoint.
  int iteration = 0;
  bool changed = true;
  while (changed) {
    if (debug) {
      fprintf(stderr, "info: closure:"
	      " iteration %d: %zu files, %zu dependencies\n",
	      ++iteration, closure.size(), elements);
    }

    changed = false;
    for (std::pair<const database::file_id, std::set<database::file_id>>
	   &needing : closure) {
      std::set<database::file_id> &needing_deps(needing.second);
      for (database::file_id needing_dep : needing_deps) {
	dependency_map::const_iterator dep(closure.find(needing_dep));
	if (dep != closure.end()) {
	  for (database::file_id depdep : dep->second) {
	    bool new_element = needing_deps.insert(depdep).second;
	    elements += new_element;
	    changed = changed || new_element;
	  }
	}
      }
    }
  }
  if (debug) {
    fprintf(stderr, "info: closure: finished\n");
  }
  arch_soname.clear();
  res.close();

  if (conflicts && conflicts->skip_update()) {
    return;
  }

  // Load the closure into the database.
  res.exec(conn, "CREATE TEMPORARY TABLE update_elf_closure ("
	   " file_id INTEGER NOT NULL,"
	   " needed INTEGER NOT NULL) ON COMMIT DROP");
  {
    std::vector<char> upload;
    pgresult_handle copy;
    copy.exec(conn, "COPY update_elf_closure FROM STDIN");
    assert(copy.resultStatus() == PGRES_COPY_IN);
    for (std::pair<const database::file_id, std::set<database::file_id>>
	   &needing : closure) {
      char filebuf[32];
      snprintf(filebuf, sizeof(filebuf), "%d", needing.first.value());
      char *fileend = filebuf + strlen(filebuf);
      std::set<database::file_id> &needing_deps(needing.second);
      for (database::file_id needed : needing_deps) {
	char neededbuf[32];
	snprintf(neededbuf, sizeof(neededbuf), "%d", needed.value());
	char *neededend = neededbuf + strlen(neededbuf);
	upload.insert(upload.end(), filebuf, fileend);
	upload.push_back('\t');
	upload.insert(upload.end(), neededbuf, neededend);
	upload.push_back('\n');
	if (upload.size() > 128 * 1024) {
	  conn.putCopyData(upload.data(), upload.size());
	  upload.clear();
	}
      }
    }
    if (!upload.empty()) {
      conn.putCopyData(upload.data(), upload.size());
    }
    conn.putCopyEnd();
    copy.getresult(conn);
  }
  res.exec(conn, "CREATE INDEX ON update_elf_closure (file_id, needed)");
  res.exec(conn, "ANALYZE update_elf_closure");
  pg_query(conn, res,
	   "DELETE FROM symboldb.elf_closure ec"
	   " WHERE set_id = $1"
	   " AND NOT EXISTS (SELECT 1 FROM update_elf_closure u"
	   "  WHERE ec.file_id = u.file_id AND ec.needed = u.needed)",
	   id.value());
  pg_query(conn, res,
	   "INSERT INTO symboldb.elf_closure (set_id, file_id, needed)"
	   " SELECT $1, * FROM (SELECT * FROM update_elf_closure"
	   " EXCEPT SELECT file_id, needed FROM symboldb.elf_closure"
	   " WHERE set_id = $1) x", id.value());
  res.exec(conn, "DROP TABLE update_elf_closure");
}

//////////////////////////////////////////////////////////////////////
// update_elf_closure_conflicts

bool
update_elf_closure_conflicts::skip_update()
{
  return false;
}

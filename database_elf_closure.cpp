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

#include "database_elf_closure.hpp"
#include "pgresult_handle.hpp"
#include "pgconn_handle.hpp"
#include "pg_exception.hpp"
#include "pg_query.hpp"
#include "pg_response.hpp"

#include <map>
#include <set>

#include <cassert>
#include <cstring>

namespace {
  struct file_ref {
    database::file_id id;
    std::string name;
    file_ref(int fid, const std::string &file_name)
      : id(fid), name(file_name)
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
    if (leftslash == NULL || rightslash == NULL
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
    for (unsigned i = 0; name[i] && name[i] == needed_path[i]; ++i) {
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
      if (prio > best_priority) {
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
 }

void
update_elf_closure(pgconn_handle &conn, database::package_set_id id,
		   update_elf_closure_conflicts *conflicts)
{
  assert(conn.transactionStatus() == PQTRANS_INTRANS);
  bool debug = false;

  // Obtain the list of SONAME providers.  There can be multiple DSOs
  // which have the same SONAME, and packages can conflict and install
  // different files at the same path.
  // FIXME: Providers should be restricted to DYN ELF images.
  pgresult_handle res;
  pg_query_binary(conn, res,
		  "SELECT ef.arch::text, ef.soname, f.id, f.name"
		  " FROM symboldb.package_set_member psm"
		  " JOIN symboldb.file f ON psm.package = f.package"
		  " JOIN symboldb.elf_file ef ON f.id = ef.file"
		  " WHERE psm.set = $1", id.value());

  arch_soname_map arch_soname;
  {
    std::string arch;
    std::string soname;
    int fid;
    std::string file_name;
    for (int row = 0, end = res.ntuples(); row < end; ++row) {
      pg_response(res, row, arch, soname, fid, file_name);
      arch_soname[arch].insert(std::make_pair(soname,
					      file_ref(fid, file_name)));
    }
  }

  dependency_map closure;
  pg_query_binary
    (conn, res,
     "SELECT ef.arch::text, en.name, f.id, f.name"
     " FROM symboldb.package_set_member psm"
     " JOIN symboldb.file f ON psm.package = f.package"
     " JOIN symboldb.elf_file ef ON f.id = ef.file"
     " JOIN symboldb.elf_needed en ON f.id = en.file"
     " WHERE psm.set = $1", id.value());
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
    for (dependency_map::iterator
	   needing = closure.begin(), needing_end = closure.end();
	 needing != needing_end; ++needing) {
      std::set<database::file_id> &needing_deps(needing->second);
      for (std::set<database::file_id>::const_iterator
	     needing_dep = needing_deps.begin(),
	     needing_dep_end = needing_deps.end();
	   needing_dep != needing_dep_end; ++needing_dep) {
	dependency_map::const_iterator dep(closure.find(*needing_dep));
	if (dep != closure.end()) {
	  for (std::set<database::file_id>::const_iterator
		 depdep = dep->second.begin(), depdep_end = dep->second.end();
	       depdep != depdep_end; ++depdep) {
	    bool new_element = needing_deps.insert(*depdep).second;
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
  char idstr[32];
  snprintf(idstr, sizeof(idstr), "%d", id.value());
  char *idstrend = idstr + strlen(idstr);
  std::vector<char> upload;
  pgresult_handle copy;
  pg_query
    (conn, copy, "DELETE FROM symboldb.elf_closure WHERE package_set = $1",
     id.value());
  copy.exec(conn, "COPY symboldb.elf_closure FROM STDIN");
  assert(copy.resultStatus() == PGRES_COPY_IN);
  for (dependency_map::iterator
	 needing = closure.begin(), needing_end = closure.end();
       needing != needing_end; ++needing) {
    char filebuf[32];
    snprintf(filebuf, sizeof(filebuf), "%d", needing->first.value());
    char *fileend = filebuf + strlen(filebuf);
    std::set<database::file_id> &needing_deps(needing->second);
    for (std::set<database::file_id>::const_iterator
	   needing_dep = needing_deps.begin(),
	   needing_dep_end = needing_deps.end();
	 needing_dep != needing_dep_end; ++needing_dep) {
      database::file_id needed(needing_dep->value());
      char neededbuf[32];
      snprintf(neededbuf, sizeof(neededbuf), "%d", needed.value());
      char *neededend = neededbuf + strlen(neededbuf);

      upload.insert(upload.end(), idstr, idstrend);
      upload.push_back('\t');
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

//////////////////////////////////////////////////////////////////////
// update_elf_closure_conflicts

bool
update_elf_closure_conflicts::skip_update()
{
  return false;
}

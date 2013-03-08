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

#include <map>
#include <set>

#include <cassert>
#include <cstring>

namespace {
  struct less_than_str {
    bool operator()(const char *left, const char *right) const
    {
      return strcmp(left, right) < 0;
    }
  };

  struct string_file_id {};
  typedef tagged<const char *, string_file_id> file_id;

  struct less_than_file {
    bool operator()(file_id left, file_id right) const
    {
      return strcmp(left.value(), right.value()) < 0;
    }
  };

  struct file_ref {
    file_id id;
    const char *name;
    file_ref(const pgresult_handle &res, int row)
      : id(res.getvalue(row, 2)),
	name(res.getvalue(row, 3))
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
    if (strncmp(name, "/lib/", 5) == 0
	|| strncmp(name, "/lib64/", 7) == 0
	|| strncmp(name, "/usr/lib/", 9) == 0
	|| strncmp(name, "/usr/lib64/", 11) == 0) {
	prio += lib_prio;
    }
    if (same_directory(name, needed_path)) {
      prio += directory_prio;
    }
    // Prefer libraries in the same file system area, with a shared
    // initial path.
    for (unsigned i = 0; name[i] && name[i] == needed_path[i]; ++i) {
      prio += 2;
    }
    // Deeply nested libraries are less prefered.
    prio -= strlen(name);
    return prio;
  }

  typedef std::multimap<const char *, file_ref, less_than_str> soname_map;
  typedef std::map<const char *, soname_map, less_than_str> arch_soname_map;
  typedef std::map<file_id, std::set<file_id, less_than_file>, less_than_file >
    dependency_map;

  // Find the most suitable library for a particular SONAME reference.
  file_id
  lookup(const arch_soname_map &arch_soname,
	 const char *arch, const char *needed_name, const char *needing_path,
	 bool debug = false)
  {
    arch_soname_map::const_iterator soname(arch_soname.find(arch));
    if (soname == arch_soname.end()) {
      return file_id();
    }
    std::pair<soname_map::const_iterator, soname_map::const_iterator> providers
      (soname->second.equal_range(needed_name));
    if (providers.first == providers.second) {
      return file_id();
    }

    soname_map::const_iterator &p(providers.first);
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
	      needing_path, needed_name);
      fprintf(stderr, "info: closure:     %s %d\n", best->name, best_priority);
    }
    do {
      int prio = p->second.priority(needing_path);
      if (debug) {
	fprintf(stderr, "info: closure:     %s %d\n", p->second.name, prio);
      }
      if (prio > best_priority) {
	best = &p->second;
	best_priority = prio;
      }
      ++p;
    } while (p != providers.second);
    if (debug) {
      fprintf(stderr, "info: closure:   winner: %s %d\n",
	      best->name, best_priority);
    }
    return best->id;
  }
 }

void
update_elf_closure(pgconn_handle &conn, database::package_set_id id)
{
  assert(conn.transactionStatus() == PQTRANS_INTRANS);
  bool debug = false;

  char idstr[32];
  snprintf(idstr, sizeof(idstr), "%d", id.value());
  const char *idParams[] = {idstr};

  // Obtain the list of SONAME providers.  There can be multiple DSOs
  // which have the sane SONAME.
  // FIXME: Providers should be restricted to DYN ELF images.
  pgresult_handle soname_provider_result;
  soname_provider_result.execParams
    (conn,
     "SELECT ef.arch, ef.soname, f.id, f.name"
     " FROM symboldb.package_set_member psm"
     " JOIN symboldb.file f ON psm.package = f.package"
     " JOIN symboldb.elf_file ef ON f.id = ef.file"
     " WHERE psm.set = $1", idParams);

  arch_soname_map arch_soname;
  for (int i = 0, end = soname_provider_result.ntuples(); i < end; ++i) {
    const char *arch = soname_provider_result.getvalue(i, 0);
    const char *soname = soname_provider_result.getvalue(i, 1);
    arch_soname[arch].insert(std::make_pair
			     (soname, file_ref(soname_provider_result, i)));
  }

  dependency_map closure;
  pgresult_handle needed_result;
  needed_result.execParams
    (conn,
     "SELECT ef.arch, en.name, f.id, f.name"
     " FROM symboldb.package_set_member psm"
     " JOIN symboldb.file f ON psm.package = f.package"
     " JOIN symboldb.elf_file ef ON f.id = ef.file"
     " JOIN symboldb.elf_needed en ON f.id = en.file"
     " WHERE psm.set = $1", idParams);
  size_t elements = 0;
  for (int i = 0, end = needed_result.ntuples(); i < end; ++i) {
    const char *arch = needed_result.getvalue(i, 0);
    const char *needed_name = needed_result.getvalue(i, 1);
    file_id needing_file(needed_result.getvalue(i, 2));
    const char *needing_path = needed_result.getvalue(i, 3);
    file_id library = lookup(arch_soname, arch, needed_name, needing_path);
    if (library != file_id()) {
      elements += closure[needing_file].insert(library).second;
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
      std::set<file_id, less_than_file> &needing_deps(needing->second);
      for (std::set<file_id>::const_iterator
	     needing_dep = needing_deps.begin(),
	     needing_dep_end = needing_deps.end();
	   needing_dep != needing_dep_end; ++needing_dep) {
	dependency_map::const_iterator dep(closure.find(*needing_dep));
	if (dep != closure.end()) {
	  for (std::set<file_id>::const_iterator
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

  // No longer needed.  We need to keep the pgresult_handles around
  // because we have pointers into their data.
  arch_soname.clear();

  // Load the closure into the database.
  char *idstrend = idstr + strlen(idstr);
  std::vector<char> upload;
  pgresult_handle copy;
  copy.execParams
    (conn, "DELETE FROM symboldb.elf_closure WHERE package_set = $1", idParams);
  copy.exec(conn, "COPY symboldb.elf_closure FROM STDIN");
  assert(copy.resultStatus() == PGRES_COPY_IN);
  for (dependency_map::iterator
	 needing = closure.begin(), needing_end = closure.end();
       needing != needing_end; ++needing) {
    const char *file = needing->first.value();
    const char *file_end = file + strlen(file);
    std::set<file_id, less_than_file> &needing_deps(needing->second);
    for (std::set<file_id>::const_iterator
	   needing_dep = needing_deps.begin(),
	   needing_dep_end = needing_deps.end();
	 needing_dep != needing_dep_end; ++needing_dep) {
      const char *needed = needing_dep->value();
      const char *needed_end = needed + strlen(needed);

      upload.insert(upload.end(), idstr, idstrend);
      upload.push_back('\t');
      upload.insert(upload.end(), file, file_end);
      upload.push_back('\t');
      upload.insert(upload.end(), needed, needed_end);
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

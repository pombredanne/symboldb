/*
 * Copyright (C) 2013 Red Hat, Inc.
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

#include "symboldb_download_repo.hpp"
#include "symboldb_options.hpp"
#include "database.hpp"
#include "package_set_consolidator.hpp"
#include "repomd.hpp"
#include "file_cache.hpp"
#include "rpm_load.hpp"

#include <cstdio>
#include <set>
#include <vector>

namespace {
  struct rpm_url {
    std::string href;
    checksum csum;
  };
}

int
symboldb_download_repo(const symboldb_options &opt, database &db,
		       char **argv, bool load)
{
  database::package_set_id set;;
  if (load && !opt.set_name.empty()) {
    set = db.lookup_package_set(opt.set_name.c_str());
    if (set == database::package_set_id()) {
      fprintf(stderr, "error: unknown package set: %s\n",
	      opt.set_name.c_str());
      return 1;
    }
  }

  std::tr1::shared_ptr<file_cache> fcache(opt.rpm_cache());
  package_set_consolidator<rpm_url> pset;

  for (; *argv; ++ argv) {
    const char *url = *argv;
    if (opt.output != symboldb_options::quiet) {
      fprintf(stderr, "info: processing repository %s\n", url);
    }
    repomd rp;
    rp.acquire(opt.download(), db, url);
    repomd::primary_xml primary_xml(rp, opt.download_always_cache(), db);
    repomd::primary primary(&primary_xml, rp.base_url.c_str());
    while (primary.next()) {
      rpm_url rurl;
      rurl.href = primary.href();
      rurl.csum = primary.checksum();
      pset.add(primary.info(), rurl);
    }
  }

  // Cache bypass for RPM downloads.
  download_options dopts_no_cache;
  dopts_no_cache.cache_mode = download_options::no_cache;
  std::string error;

  std::vector<rpm_url> urls(pset.values());
  if (opt.output != symboldb_options::quiet) {
    fprintf(stderr, "info: %zu packages in download set\n", urls.size());
  }
  size_t download_count = 0;
  std::set<database::package_id> pids;

  for (std::vector<rpm_url>::iterator p = urls.begin(), end = urls.end();
       p != end; ++p) {
    if (load) {
      database::package_id pid = db.package_by_digest(p->csum.value);
      if (pid != database::package_id()) {
	if (opt.output == symboldb_options::verbose) {
	  fprintf(stderr, "info: skipping %s\n", p->href.c_str());
	}
	pids.insert(pid);
	continue;
      }
    }

    std::string rpm_path;
    try {
      database::advisory_lock lock
	(db.lock_digest(p->csum.value.begin(), p->csum.value.end()));
      if (!fcache->lookup_path(p->csum, rpm_path)) {
	if (opt.output != symboldb_options::quiet) {
	  fprintf(stderr, "info: downloading %s\n", p->href.c_str());
	}
	++download_count;
	file_cache::add_sink sink(*fcache, p->csum);
	download(dopts_no_cache, db, p->href.c_str(), &sink);
	sink.finish(rpm_path);
      }
    } catch (file_cache::unsupported_hash &e) {
      fprintf(stderr, "error: unsupported hash for %s: %s\n",
	      p->href.c_str(), e.what());
      return 1;
    } catch (file_cache::checksum_mismatch &e) {
      fprintf(stderr, "error: checksum mismatch for %s: %s\n",
	      p->href.c_str(), e.what());
      return 1;
    }

    if (load) {
      rpm_package_info info;
      database::package_id pid = rpm_load(opt, db, rpm_path.c_str(), info);
      if (pid == database::package_id()) {
	return 1;
      }
      pids.insert(pid);
    }
  }
  if (opt.output != symboldb_options::quiet) {
    fprintf(stderr, "info: downloaded %zu of %zu packages\n",
	    download_count, urls.size());
  }
  if (load && set > database::package_set_id()) {
    db.txn_begin();
    {
      database::advisory_lock lock
	(db.lock(database::PACKAGE_SET_LOCK_TAG, set.value()));
      if (db.update_package_set(set, pids.begin(), pids.end())) {
	finalize_package_set(opt, db, set);
      }
    }
    db.txn_commit();
  }

  return 0;
}

void
finalize_package_set(const symboldb_options &opt, database &db,
		     database::package_set_id set)
{
  if (opt.output != symboldb_options::quiet) {
    fprintf(stderr, "info: updating package set caches\n");
  }
  db.update_package_set_caches(set);
}

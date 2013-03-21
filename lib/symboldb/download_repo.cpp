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

#include <symboldb/download_repo.hpp>
#include <symboldb/options.hpp>
#include <symboldb/database.hpp>
#include <cxxll/package_set_consolidator.hpp>
#include <symboldb/repomd.hpp>
#include <cxxll/file_cache.hpp>
#include <symboldb/rpm_load.hpp>
#include <cxxll/curl_exception.hpp>
#include <cxxll/curl_exception_dump.hpp>
#include <cxxll/regex_handle.hpp>

#include <algorithm>
#include <cstdio>
#include <set>
#include <vector>

using namespace cxxll;

namespace {
  struct rpm_url {
    std::string name;
    std::string href;
    checksum csum;
  };

  //////////////////////////////////////////////////////////////////////
  // name_filter

  struct name_filter {
    regex_handle rx_;
    size_t &count_;

    name_filter(const symboldb_options &, size_t &count);
    bool operator()(const rpm_url &);
  };

  inline
  name_filter::name_filter(const symboldb_options &opt, size_t &count)
    : rx_(opt.exclude_name()), count_(count)
  {
  }

  inline bool
  name_filter::operator()(const rpm_url &rurl)
  {
    if (rx_.match(rurl.name.c_str())) {
      ++count_;
      return true;
    } else {
      return false;
    }
  }

  //////////////////////////////////////////////////////////////////////
  // database_filter

  struct database_filter {
    const symboldb_options &opt_;
    database &db_;
    std::set<database::package_id> &pids_;

    database_filter(const symboldb_options &, database &,
		    std::set<database::package_id> &);
    bool operator()(const rpm_url &);
  };

  inline
  database_filter::database_filter(const symboldb_options &opt, database &db,
				   std::set<database::package_id> &pids)
    : opt_(opt), db_(db), pids_(pids)
  {
  }

  inline bool
  database_filter::operator()(const rpm_url &rurl)
  {
    database::package_id pid = db_.package_by_digest(rurl.csum.value);
    if (pid != database::package_id()) {
      if (opt_.output == symboldb_options::verbose) {
	fprintf(stderr, "info: skipping %s\n", rurl.href.c_str());
      }
      pids_.insert(pid);
      return true;
    }
    return false;
  }

  //////////////////////////////////////////////////////////////////////
  // download_filter

  struct download_filter : database_filter {
    std::tr1::shared_ptr<file_cache> fcache_;
    size_t &count_;
    bool load_;

    // Cache bypass for RPM downloads.
    download_options dopts_no_cache_;

    download_filter(const symboldb_options &, database &,
		    std::set<database::package_id> &,
		    size_t &count, bool load);
    bool operator()(const rpm_url &);
  };

  inline
  download_filter::download_filter(const symboldb_options &opt, database &db,
				   std::set<database::package_id> &pids,
				   size_t &count, bool load)
    : database_filter(opt, db, pids),
      fcache_(opt.rpm_cache()), count_(count), load_(load)
  {
    dopts_no_cache_.cache_mode = download_options::no_cache;
  }

  inline bool
  download_filter::operator()(const rpm_url &rurl)
  {
    try {
      std::string rpm_path;
      database::advisory_lock lock
	(db_.lock_digest(rurl.csum.value.begin(), rurl.csum.value.end()));
      if (database_filter::operator()(rurl)) {
	return true;
      } else if (!fcache_->lookup_path(rurl.csum, rpm_path)) {
	if (opt_.output != symboldb_options::quiet) {
	  if (rurl.csum.length != checksum::no_length) {
	    fprintf(stderr, "info: downloading %s (%llu bytes)\n",
		    rurl.href.c_str(), rurl.csum.length);
	  } else {
	    fprintf(stderr, "info: downloading %s\n", rurl.href.c_str());
	  }
	}
	file_cache::add_sink sink(*fcache_, rurl.csum);
	download(dopts_no_cache_, db_, rurl.href.c_str(), &sink);
	sink.finish(rpm_path);
	++count_;
      }

      if (load_) {
	rpm_package_info info;
	database::package_id pid = rpm_load(opt_, db_, rpm_path.c_str(), info,
					    &rurl.csum);
	if (pid == database::package_id()) {
	  return false;
	}
	pids_.insert(pid);
      }
      return true;
    } catch (curl_exception &e) {
      dump("error: ", e, stderr);
      return false;
    } catch (file_cache::unsupported_hash &e) {
      fprintf(stderr, "error: unsupported hash for %s: %s\n",
	      rurl.href.c_str(), e.what());
      return false;
    } catch (file_cache::checksum_mismatch &e) {
      fprintf(stderr, "error: checksum mismatch for %s: %s\n",
	      rurl.href.c_str(), e.what());
      return false;
    }
  }
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
      rurl.name = primary.info().name;
      rurl.href = primary.href();
      rurl.csum = primary.checksum();
      pset.add(primary.info(), rurl);
    }
  }

  std::vector<rpm_url> urls(pset.values());
  if (opt.output != symboldb_options::quiet) {
    fprintf(stderr, "info: %zu packages in download set\n", urls.size());
  }
  std::set<database::package_id> pids;

  if (opt.exclude_name_present()) {
    size_t count = 0;
    name_filter filter(opt, count);
    std::vector<rpm_url>::iterator p = std::remove_if
      (urls.begin(), urls.end(), filter);
    urls.erase(p, urls.end());
    if (opt.output != symboldb_options::quiet) {
      fprintf(stderr, "info: package name filter excluded %zu packages\n",
	      count);
    }
  }

  {
    database_filter filter(opt, db, pids);
    std::vector<rpm_url>::iterator p = std::remove_if
      (urls.begin(), urls.end(), filter);
    urls.erase(p, urls.end());
    if (opt.output != symboldb_options::quiet) {
      fprintf(stderr,
	      "info: %zu packages to download after database comparison\n",
	      urls.size());
    }
  }

  {
    size_t start_count = urls.size();
    size_t download_count = 0;
    download_filter filter(opt, db, pids, download_count, load);
    for (unsigned iteration = 1;
	 iteration <= 3 && !urls.empty(); ++iteration) {
      if (opt.randomize) {
	std::random_shuffle(urls.begin(), urls.end());
      }
      std::vector<rpm_url>::iterator p = std::remove_if
	(urls.begin(), urls.end(), filter);
      urls.erase(p, urls.end());
    }
    if (opt.output != symboldb_options::quiet) {
      fprintf(stderr, "info: downloaded %zu of %zu packages\n",
	      download_count, start_count);
    }
  }

  bool do_pset_update = load && set != database::package_set_id();

  if (!urls.empty()) {
    fprintf(stderr, "error: %zu packages failed download:\n", urls.size());
    for (std::vector<rpm_url>::iterator p = urls.begin(), end = urls.end();
	 p != end; ++p) {
      fprintf(stderr, "error:   %s\n", p->href.c_str());
    }
    if (opt.ignore_download_errors && do_pset_update) {
      if (pids.empty()) {
	fprintf(stderr, "error: no packages left in download set\n");
      } else {
	fprintf(stderr, "warning: download errors ignored, continuing\n");
      }
    } else {
      return 1;
    }
  }

  if (do_pset_update) {
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

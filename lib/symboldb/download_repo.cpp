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
#include <cxxll/task.hpp>
#include <cxxll/mutex.hpp>
#include <cxxll/bounded_ordered_queue.hpp>

#include <algorithm>
#include <cassert>
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
  // downloader

  struct downloader {
    // Tries to perform all the downloads in the vector.  Afterwards,
    // urls_ contains those URLs for which the download failed.
    downloader(const symboldb_options &, database &,
	       std::set<database::package_id> &,
	       std::vector<rpm_url> &, bool load);

    size_t count() const;

  private:
    const symboldb_options &opt_;

    // The following are guarded by mutex_.
    std::set<database::package_id> &pids_;
    std::vector<rpm_url> &urls_;
    std::vector<rpm_url> failed_urls_;
    mutex mutex_;

    struct load_info {
      std::string rpm_path; // path in the file system
      checksum csum;
    };
    bounded_ordered_queue<std::string, load_info> queue_; // key: RPM name
    size_t count_;

    const bool load_;

    // Cache bypass for RPM downloads.
    download_options dopts_no_cache_;

    // Called by the constructor to do the actual work.
    void process(database &);

    // The individual download helper tasks.
    void download_helper_task();

    // Called by download_helper_task() to download one URL.
    void download_url(database &, file_cache &, const rpm_url &);

    // Called by download_url() to skip URLs already in the database.
    bool download_fast_track(database &, const rpm_url &);

    static mutex stderr_mutex;
  };

  mutex downloader::stderr_mutex;

  downloader::downloader(const symboldb_options &opt, database &db,
			 std::set<database::package_id> &pids,
			 std::vector<rpm_url> &urls, bool load)
    : opt_(opt), pids_(pids), urls_(urls),
      queue_(opt.download_threads, 0), count_(0), load_(load)
  {
    dopts_no_cache_.cache_mode = download_options::no_cache;
    process(db);
  }

  size_t
  downloader::count() const
  {
    return count_;
  }

  void
  downloader::process(database &db)
  {
    // download_helper_task() removes URLs from the pack of the vector.
    std::reverse(urls_.begin(), urls_.end());

    // Retry three times or until we downloaded all URLs.
    for (int round = 0; round < 3; ++ round) {
      std::vector<std::tr1::shared_ptr<task> > tasks;
      for (unsigned tid = 0; tid < opt_.download_threads; ++tid) {
	queue_.add_producer();
	std::tr1::shared_ptr<task> dtask
	  (new task(std::tr1::bind(&downloader::download_helper_task, this)));
	tasks.push_back(dtask);
      }
      std::string name;
      load_info to_load;
      while (queue_.pop(name, to_load)) {
	++count_;
	if (load_) {
	  rpm_package_info info;
	  database::package_id pid = rpm_load
	    (opt_, db, to_load.rpm_path.c_str(), info, &to_load.csum);
	  assert(pid != database::package_id());
	  mutex::locker ml(&mutex_);
	  pids_.insert(pid);
	}
      }

      assert(queue_.producers() == 0);
      assert(urls_.empty());
      if (failed_urls_.empty()) {
	break;
      }
      // Retry failed URLs.
      std::swap(failed_urls_, urls_);
    }
  }

  void
  downloader::download_helper_task()
  {
    // Per-task database and cache objects.
    database db;
    std::tr1::shared_ptr<file_cache> fcache(opt_.rpm_cache());

    while (true) {
      rpm_url url;
      {
	mutex::locker ml(&mutex_);
	if (urls_.empty()) {
	  break;
	}
	url = urls_.back();
	urls_.pop_back();
      }
      download_url(db, *fcache, url);
    }
    queue_.remove_producer();
  }

  bool
  downloader::download_fast_track(database &db, const rpm_url &url)
  {
    database::package_id pid = db.package_by_digest(url.csum.value);
    if (pid != database::package_id()) {
      if (opt_.output == symboldb_options::verbose) {
	mutex::locker ml(&stderr_mutex);
	fprintf(stderr, "info: skipping %s\n", url.href.c_str());
      }
      mutex::locker ml(&mutex_);
      pids_.insert(pid);
      return true;
    }
    return false;
  }

  void
  downloader::download_url(database &db, file_cache &fcache, const rpm_url &url)
  {
    database::advisory_lock lock
      (db.lock_digest(url.csum.value.begin(), url.csum.value.end()));
    if (download_fast_track(db, url)) {
      return;
    }
    load_info to_load;
    to_load.csum = url.csum;
    if (!fcache.lookup_path(url.csum, to_load.rpm_path)) {
      if (opt_.output != symboldb_options::quiet) {
	mutex::locker ml(&stderr_mutex);
	if (url.csum.length != checksum::no_length) {
	  fprintf(stderr, "info: downloading %s (%llu bytes)\n",
		  url.href.c_str(), url.csum.length);
	} else {
	  fprintf(stderr, "info: downloading %s\n", url.href.c_str());
	}
      }
      bool good = false;
      try {
	file_cache::add_sink sink(fcache, url.csum);
	download(dopts_no_cache_, db, url.href.c_str(), &sink);
	sink.finish(to_load.rpm_path);
	good = true;
      } catch (curl_exception &e) {
	mutex::locker ml(&stderr_mutex);
	dump("error: ", e, stderr);
      } catch (file_cache::unsupported_hash &e) {
	mutex::locker ml(&stderr_mutex);
	fprintf(stderr, "error: unsupported hash for %s: %s\n",
		url.href.c_str(), e.what());
      } catch (file_cache::checksum_mismatch &e) {
	mutex::locker ml(&stderr_mutex);
	fprintf(stderr, "error: checksum mismatch for %s: %s\n",
		url.href.c_str(), e.what());
      }
      if (good) {
	queue_.push(url.name, to_load);
      } else {
	mutex::locker ml(&mutex_);
	failed_urls_.push_back(url);
	return;
      }
    }
  }

} // anonymous namespace

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

  if (opt.randomize) {
    std::random_shuffle(urls.begin(), urls.end());
  }

  {
    size_t start_count = urls.size();
    downloader dl(opt, db, pids, urls, load);
    if (opt.output != symboldb_options::quiet) {
      fprintf(stderr, "info: downloaded %zu of %zu packages\n",
	      dl.count(), start_count);
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

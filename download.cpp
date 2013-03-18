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

#include "download.hpp"
#include "database.hpp"
#include <cxxll/curl_exception.hpp>
#include <cxxll/curl_fetch_result.hpp>
#include <cxxll/vector_sink.hpp>

download_options::download_options()
  : cache_mode(check_cache)
{
}

void
download(const download_options &opt, database &db,
	 const char *url, sink *target)
{
  switch (opt.cache_mode) {
  case download_options::only_cache:
  case download_options::always_cache:
    {
      std::vector<unsigned char> data;
      if (db.url_cache_fetch(url, data)) {
	target->write(data.data(), data.size());
	return;
      }
      if (opt.cache_mode == download_options::only_cache) {
	throw curl_exception("URL not in cache and network access disabled")
	  .url(url);
      }
    }
    break;
  case download_options::no_cache:
    break;
  case download_options::check_cache:
    {
      vector_sink vsink;
      curl_fetch_result r(&vsink);
      r.head(url);
      if (r.http_date > 0 && r.http_size >= 0
	  && db.url_cache_fetch(url, static_cast<size_t>(r.http_size),
				r.http_date, vsink.data)) {
	target->write(vsink.data.data(), vsink.data.size());
	return;
      }
    }
  }

  curl_fetch_result r(target);
  r.get(url);
  if (opt.cache_mode != download_options::no_cache) {
    if (vector_sink *vsink = dynamic_cast<vector_sink *>(target)) {
      db.url_cache_update(url, vsink->data, r.http_date);
    }
  }
}

void
download(const download_options &opt, database &db,
	 const char *url, std::vector<unsigned char> &result)
{
  vector_sink target;
  target.data.swap(result);
  download(opt, db, url, &target);
  target.data.swap(result);
}

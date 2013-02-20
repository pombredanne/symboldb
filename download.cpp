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
#include "curl_fetch_result.hpp"
#include "vector_sink.hpp"

download_options::download_options()
  : cache_mode(check_cache)
{
}

bool
download(const download_options &opt, database &db,
	 const char *url, std::vector<unsigned char> &result,
	 std::string &error)
{
  switch (opt.cache_mode) {
  case download_options::only_cache:
  case download_options::always_cache:
    if (db.url_cache_fetch(url, result)) {
      return true;
    }
    if (opt.cache_mode == download_options::only_cache) {
      error = "URL not in cache and network access disabled";
      return false;
    }
    break;
  case download_options::no_cache:
    break;
  case download_options::check_cache:
    {
      vector_sink target;
      curl_fetch_result r(&target);
      r.head(url);
      target.data.clear();
      if (r.error.empty()
	  && r.http_date > 0 && r.http_size >= 0
	  && db.url_cache_fetch(url, static_cast<size_t>(r.http_size),
				r.http_date, target.data)) {
	result.swap(target.data);
	return true;
      }
    }
  }

  vector_sink target;
  curl_fetch_result r(&target);
  r.get(url);
  if (!r.error.empty()) {
    error.swap(r.error);
    return false;
  }
  if (opt.cache_mode != download_options::no_cache) {
    db.url_cache_update(url, target.data, r.http_date);
  }
  result.swap(target.data);
  return true;
}

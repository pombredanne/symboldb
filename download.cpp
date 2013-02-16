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

bool
download(database &db, const char *url, std::vector<unsigned char> &result,
	 std::string &error)
{
  curl_fetch_result r;
  r.head(url);
  if (r.error.empty()
      && r.http_date > 0 && r.http_size >= 0
      && db.url_cache_fetch(url, static_cast<size_t>(r.http_size),
			    r.http_date, r.data)) {
    result.swap(r.data);
    return true;
  }

  // Error or cache miss.
  r.get(url);
  if (!r.error.empty()) {
    error.swap(r.error);
    return false;
  }
  // FIXME: Perhaps we should not store anything in the database if
  // the previous HEAD request did not return a length or a time
  // stamp.
  db.url_cache_update(url, r.data, r.http_date);
  result.swap(r.data);
  return true;
}


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

#pragma once

#include <string>
#include <vector>

class database;
class sink;

struct download_options {
  enum {
    no_cache,			// do not use the cache
    check_cache,		// use the cache if it is current
    always_cache,		// always use the cache if available
    only_cache			// only use the cache, no network
  } cache_mode;

  download_options();
};

// Tries to download URL and sends its contents to SINK.  Throws
// pg_exception or curl_exception on errors, or whatever SINK throws.
void download(const download_options &, database &,
	      const char *url, sink *);

// Tries to download URL and stores its contents in RESULT.  Throws
// pg_exception or curl_exception on errors.
void download(const download_options &, database &,
	      const char *url, std::vector<unsigned char> &result);

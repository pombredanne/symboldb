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

class sink;

#include <string>
#include <vector>

class curl_handle;

struct curl_fetch_result {
  std::string effective_url;
  sink *target;	             // receives the data
  long http_date;	     // last modification date, -1 if not available
  long long http_size;	     // file size on the server, -1 if not available

  curl_fetch_result(sink *); // initializes target
  ~curl_fetch_result();

  // Performs a GET request for the URL, using reasonable defaults.
  // May throw std::bad_alloc.  Other errors are indicated through
  // the http_status/error fields.
  void get(const char *url);

  // Performs a HEAD request for the URL, using reasonable defaults.
  // May throw std::bad_alloc.  Other errors are indicated through
  // the http_status/error fields.
  void head(const char *url);

private:
  std::string error;		// records errors from the callback
  static size_t write_function(char *, size_t, size_t, void *userdata);
  void init(curl_handle &, const char *url);
  void perform(curl_handle &, const char *url);
};

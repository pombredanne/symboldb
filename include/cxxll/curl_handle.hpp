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

#include <curl/curl.h>

struct curl_handle {
  CURL *raw;

  // Initializes the raw pointer with the result of calling
  // curl_easy_init.  Throws std::bad_alloc on failure.
  curl_handle();

  // Deallocates the raw pointer.  Assign NULL to the raw pointer to
  // prevent that.
  ~curl_handle();

private:
  curl_handle(const curl_handle&); // not implemented
  curl_handle &operator=(const curl_handle&); // not implemented
};

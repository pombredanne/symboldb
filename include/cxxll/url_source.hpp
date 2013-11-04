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

#include <cxxll/source.hpp>

#include <string>
#include <tr1/memory>

namespace cxxll {

// Read data from a URL.
class url_source : public source {
  struct impl;
  std::tr1::shared_ptr<impl> impl_;
  url_source(const url_source &); // not implemented
  url_source &operator =(const url_source &); // not implemented
public:
  explicit url_source(const char *url);
  explicit url_source(const std::string &url);
  ~url_source();

  // Establishes the connection.  This fixes all connection
  // parameters.
  void connect();

  // Transfers data to the supplied buffer.  Calls connect() if
  // necessary.  Throws curl_exception if there is an error.
  size_t read(unsigned char *, size_t);

  // Last modification date.  Must be called after bytes have been
  // read.  Returns -1 if not available.
  long long http_date() const;

  // File size.  Must be called after bytes have been read.  Returns
  // -1 if not available.
  long long file_size() const;
};

// These functions initialize and deinitialize the underlying HTTP
// client library.
void url_source_init();
void url_source_deinit();


} // namespace cxxll

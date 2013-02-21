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

#include "sink.hpp"

#include <string>
#include <tr1/memory>
#include <vector>

class checksum;

// A cache for content-addressed files on the disk.
//
// Note that we currently do not discriminate between the digest
// types.  We currently support SHA-1 and SHA-256, so we can still
// tell them apart.
class file_cache {
  struct impl;
  std::tr1::shared_ptr<impl> impl_;
public:
  file_cache(const char *path);
  ~file_cache();

  // Returns true if the checksum is in the cache and writes the full
  // file name to PATH.
  bool lookup_path(const checksum &, std::string &path);

  // Adds the data to the cache if it does not exist yet (after
  // verifying that the checksum matches).  Updates PATH with the file
  // name.  Returns true on success, false on error (ERROR is
  // updated).
  void add(const checksum &, const std::vector<unsigned char> &data,
	   std::string &path);

  // Creates a sink for data with the specified checksum.
  // FIXME: Does not use locking yet.
  class add_sink : public sink {
    struct add_impl;
    std::tr1::shared_ptr<add_impl> impl_;
  public:
    add_sink(file_cache &, const checksum &);
    ~add_sink();

    void write(const unsigned char *, size_t);

    // Performs checksum validation
    void finish(std::string &path);
  };

  struct exception : std::exception {
    ~exception() throw();
  };

  class unsupported_hash : public exception {
    std::string hash_;
  public:
    unsupported_hash(const std::string &);
    ~unsupported_hash() throw ();
    const char *what() const throw();
  };

  // Thrown by add(), add_sink if a checksum mismatch is detected.
  class checksum_mismatch : public exception {
    const char *kind_;
  public:
    checksum_mismatch(const char *);
    ~checksum_mismatch() throw ();
    const char *what() const throw();
  };
};

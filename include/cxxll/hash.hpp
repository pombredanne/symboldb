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

#include <vector>
#include <string>
#include <memory>

namespace cxxll {

class checksum;

// Hashing sink implementation.
// On NSS errors, an exception is thrown.
class hash_sink : public sink {
  struct impl;
  std::shared_ptr<impl> impl_;
  hash_sink(const hash_sink &) = delete;
  void operator=(const hash_sink &) = delete;
public:
  typedef enum {
    md5 = 1,
    sha1,
    sha256
  } type;
  explicit hash_sink(type);
  explicit hash_sink(const char *type);
  ~hash_sink();

  // Hashes the specified byte array.
  void write(const_stringref);

  // Finalizes the hash computation and writes the 32 bytes of digest
  // to its argument.
  void digest(std::vector<unsigned char> &);

  // Returns the number of octets written so far.
  unsigned long long octets() const;

  // Convert between hash types and their string representation.
  // Throws runtime_error if the conversion fails.
  static type from_string(const char *);
  static const char *to_string(type);
};

// Computes the 32-byte SHA-256 hash of the argument.
// On NSS errors, an exception is thrown.
std::vector<unsigned char> hash(hash_sink::type,
				const std::vector<unsigned char> &data);

// Reads the file at PATH and writes its SHA-256 hash to DIGEST.  On
// NSS errors, an exception is thrown.  File system access errors
// result in os_exception.
void hash_file(hash_sink::type, const char *path,
	       std::vector<unsigned char> &digest);

// Similar to hash_file() above, except that the result is stored in a
// checksum object.
void hash_file(hash_sink::type, const char *path, checksum &);

} // namespace cxxll

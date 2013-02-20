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
#include <tr1/memory>

// Hashing sink implementation.
// On NSS errors, an exception is thrown.
class sha256_sink : public sink {
  struct impl;
  std::tr1::shared_ptr<impl> impl_;
  sha256_sink(const sha256_sink &); // not implemented
  void operator=(const sha256_sink &); // not implemented
public:
  sha256_sink();
  ~sha256_sink();

  // Hashes the specified byte array.
  void write(const unsigned char *, size_t);

  // Finalizes the hash computation and writes the 32 bytes of digest
  // to its argument.
  void digest(std::vector<unsigned char> &);
};

// Computes the 32-byte SHA-256 hash of the argument.
// On NSS errors, an exception is thrown.
std::vector<unsigned char> hash_sha256(const std::vector<unsigned char> &data);

// Reads the file at PATH and writes its SHA-256 hash to DIGEST.
// ERROR is updated in case of file system errors, and the function
// returns false.  On NSS errors, an exception is thrown.
bool hash_sha256_file(const char *path, std::vector<unsigned char> &digest,
		      std::string &error);

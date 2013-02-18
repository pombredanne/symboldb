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
#include <tr1/memory>
#include <vector>

class checksum;

// A cache for content-addressed files on the disk.
class file_cache {
  struct impl;
  std::tr1::shared_ptr<impl> impl_;
public:
  file_cache(const char *path);
  ~file_cache();

  // Returns true if the cache directory actually exists.
  bool valid();

  // Returns true if the checksum is in the cache and writes the full
  // file name to PATH.
  bool lookup_path(const checksum &, std::string &path);

  // Adds the data to the cache if it does not exist yet (after
  // verifying that the checksum matches).  Updates PATH with the file
  // name.  Returns true on success, false on error (ERROR is
  // updated).
  bool add(const checksum &, const std::vector<unsigned char> &data,
	   std::string &path, std::string &error);

  // FIXME: add an interface for streaming injection (so that the blob
  // does not have to be stored in RAM).  Optional locking against
  // concurrent addition would be desirable as well.
};

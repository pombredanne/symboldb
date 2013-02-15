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

struct checksum {
  std::string type;		// "sha256"
  unsigned long long length;	// length of data being checksummed
  std::vector<unsigned char> value; // as raw bytes (not hexadecimal)

  checksum();
  ~checksum();

  // Sets the type, performs base16 decoding, and sets value.  Returns
  // false if the checksum is malformed.
  bool set_hexadecimal(const char *type, unsigned long long length,
		       const char *checksum);
};

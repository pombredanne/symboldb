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

// Code for extracting data from vectors.

namespace cxxll {

void extract(const std::vector<unsigned char> &,
	     size_t offset, size_t length, std::string &target);

unsigned char extract8(const std::vector<unsigned char> &, size_t offset);

inline void
extract(const std::vector<unsigned char> &vec, size_t &offset,
	       unsigned char &target)
{
  size_t off = offset++;
  target = vec.at(off);
}


namespace big_endian {

unsigned short extract16(const std::vector<unsigned char> &, size_t offset);
unsigned extract32(const std::vector<unsigned char> &, size_t offset);
unsigned long long extract64(const std::vector<unsigned char> &, size_t offset);

inline void
extract(const std::vector<unsigned char> &vec, size_t &offset,
	unsigned short &target)
{
  target = extract16(vec, offset);
  offset += 2;
}

inline void
extract(const std::vector<unsigned char> &vec, size_t &offset,
	unsigned &target)
{
  target = extract32(vec, offset);
  offset += 4;
}

inline void
extract(const std::vector<unsigned char> &vec, size_t &offset,
	unsigned long long &target)
{
  target = extract64(vec, offset);
  offset += 8;
}

} // namespace cxxll::big_endian
} // namespace cxxll

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

#include <cxxll/vector_extract.hpp>
#include <cxxll/raise.hpp>

#include <stdexcept>

void
cxxll::extract(const std::vector<unsigned char> &vec,
	       size_t offset, size_t length, std::string &target)
{
  size_t end = offset + length;
  if (end < offset || end > vec.size()) {
    raise<std::out_of_range>("cxxll::extract");
  }
  std::vector<unsigned char>::const_iterator p = vec.begin();
  target.assign(p + offset, p + end);
}

unsigned char
cxxll::extract8(const std::vector<unsigned char> &vec, size_t offset)
{
  return vec.at(offset);
}

unsigned short
cxxll::big_endian::extract16(const std::vector<unsigned char> &vec,
			     size_t offset)
{
  size_t off = offset;
  unsigned a = vec.at(off);
  unsigned b = vec.at(off + 1);
  return (a << 8) + b;
}

unsigned
cxxll::big_endian::extract32(const std::vector<unsigned char> &vec,
			     size_t offset)
{
  size_t off = offset;
  unsigned a = vec.at(off);
  unsigned b = vec.at(off + 1);
  unsigned c = vec.at(off + 2);
  unsigned d = vec.at(off + 3);
  return (a << 24) + (b << 16) + (c << 8) + d;
}

unsigned long long
cxxll::big_endian::extract64(const std::vector<unsigned char> &vec,
			     size_t offset)
{
  unsigned long long a = extract32(vec, offset);
  unsigned b = extract32(vec, offset + 4);
  return (a << 32) | b;
}

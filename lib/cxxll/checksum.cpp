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

#include <cxxll/checksum.hpp>
#include <cxxll/base16.hpp>

#include <cstring>

checksum::checksum()
  : type(hash_sink::sha256)
{
}

checksum::~checksum()
{
}

const unsigned long long checksum::no_length;

namespace {
  int hexdigit(char ch)
  {
    if ('0' <= ch && ch <= '9') {
      return ch - '0';
    } else if ('a' <= ch && ch <= 'f') {
      return ch - 'a' + 10;
    } else if ('A' <= ch && ch <= 'F') {
      return ch - 'A' + 10;
    }
    return -1;
  }
}

void
checksum::set_hexadecimal(const char *typ, unsigned long long len,
			  const char *checksum)
{
  size_t cslen = std::strlen(checksum);
  std::vector<unsigned char> v;
  v.reserve(cslen / 2);
  base16_decode(checksum, checksum + cslen, std::back_inserter(v));
  type = hash_sink::from_string(typ);
  length = len;
  value.swap(v);
}

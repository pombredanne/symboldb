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

// Parses an unsigned long long, ignoring leading and trailing white space.
bool parse_unsigned_long_long(const std::string &, unsigned long long &value);

// Checks if the character is ASCII whitespace.
inline bool
whitespace(char ch)
{
  return 0 <= ch && ch <= ' ';
}

template <unsigned N> bool
ends_with(const std::string &s, const char (&pattern)[N])
{
  if (s.size() < N - 1) {
    return false;
  }
  return __builtin_memcmp(s.data() + (s.size() - N + 1), pattern, N - 1) == 0;
}


template <class InputIterator> std::string
base16_encode(InputIterator first, InputIterator last)
{
  std::string result;
  static const char digits[16] = {
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
    'a', 'b', 'c', 'd', 'e', 'f'
  };
  for (; first != last; ++first) {
    unsigned char ch = *first & 0xffU;
    result += digits[ch >> 4];
    result += digits[ch & 0x0f];
  }
  return result;
}

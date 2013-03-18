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

namespace cxxll {

// Replaces non-printable characters (less than ASCII 32, greater than
// ASCII 126) with hexadecimal escape seqences or special escape
// sequences.  Does not include the surrounding quote characters.
std::string quote(const std::string &);

// Parses an unsigned long long, ignoring leading and trailing white space.
bool parse_unsigned_long_long(const std::string &, unsigned long long &value);

// Checks if the character is ASCII whitespace.
inline bool
whitespace(char ch)
{
  return 0 <= ch && ch <= ' ';
}

// Removes leading and trailing whitespace.
std::string strip(const std::string &);

template <unsigned N> bool
starts_with(const std::string &s, const char (&pattern)[N])
{
  if (s.size() < N - 1) {
    return false;
  }
  return __builtin_memcmp(s.data(), pattern, N - 1) == 0;
}

template <unsigned N> bool
ends_with(const std::string &s, const char (&pattern)[N])
{
  if (s.size() < N - 1) {
    return false;
  }
  return __builtin_memcmp(s.data() + (s.size() - N + 1), pattern, N - 1) == 0;
}

// A simple hash function.  Use this for locking only, but not for
// hash tables.
unsigned fnv(const std::string &);
unsigned fnv(const char *);
unsigned fnv(const unsigned char *, size_t);

} // namespace cxxll

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

#include "string_support.hpp"

#include <cerrno>
#include <cstdlib>

namespace {
  const char *
  non_whitespace(const char *first, const char *last)
  {
    for (; first != last; ++first) {
      if (whitespace(*first)) {
	continue;
      }
      break;
    }
    return first;
  }
}

// Parses an unsigned long long while skipping white space.
bool
parse_unsigned_long_long(const std::string &text, unsigned long long &value)
{
  const char *first = text.c_str();
  const char *last = first + text.size();
  first = non_whitespace(first, last);
  errno = 0;
  char *endptr;
  unsigned long long v = strtoull(first, &endptr, 10);
  if (errno != 0) {
    return false;
  }
  if (non_whitespace(endptr, last) != last) {
    // Trailing non-whitespace.
    return false;
  }
  value = v;
  return true;
}

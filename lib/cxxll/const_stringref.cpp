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

#include <cxxll/const_stringref.hpp>
#include <cxxll/bad_string_index.hpp>

#include <algorithm>

void
cxxll::const_stringref::throw_bad_index(size_t size, size_t index)
{
  throw bad_string_index(size, index);
}

void
cxxll::const_stringref::throw_bad_empty()
{
  throw bad_string_index(0, 0);
}

inline int
cmp(cxxll::const_stringref left, cxxll::const_stringref right)
{
  int ret = memcmp(left.data(), right.data(),
		   std::min(left.size(), right.size()));
  if (ret != 0) {
    return ret;
  }
  if (left.size() < right.size()) {
    return -1;
  } else if (left.size() > right.size()) {
    return 1;
  } else {
    return 0;
  }
}

bool
cxxll::const_stringref::less_than(const_stringref left, const_stringref right)
{
  return cmp(left, right) < 0;
}

bool
cxxll::const_stringref::less_than_equal(const_stringref left, const_stringref right)
{
  return cmp(left, right) <= 0;
}

cxxll::const_stringref
cxxll::const_stringref::substr(cxxll::const_stringref s, size_t pos, size_t count)
{
  s.check_index_equal_ok(pos);
  return const_stringref(s.data() + pos, std::min(count, s.size() - pos));
}

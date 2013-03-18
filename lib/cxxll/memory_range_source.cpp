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

#include <cxxll/memory_range_source.hpp>

#include <algorithm>
#include <cstring>

using namespace cxxll;

memory_range_source::memory_range_source(const unsigned char *start,
					   size_t length)
  : p(start), end(start + length)
{
}

memory_range_source::~memory_range_source()
{
}

size_t
memory_range_source::read(unsigned char *buffer, size_t length)
{
  size_t to_copy = std::min(length, static_cast<size_t>(end - p));
  memcpy(buffer, p, to_copy);
  p += to_copy;
  return to_copy;
}

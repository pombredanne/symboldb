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

#include <cxxll/bad_string_index.hpp>

#include <cstdio>

static std::string
message(size_t size, size_t index)
{
  char buf[64];
  snprintf(buf, sizeof(buf), "size = %zu, index = %zu", size, index);
  return std::string(buf);
}

cxxll::bad_string_index::bad_string_index(size_t size, size_t index)
  : std::out_of_range(message(size, index)), size_(size), index_(index)
{
}

cxxll::bad_string_index::~bad_string_index() throw()
{
}

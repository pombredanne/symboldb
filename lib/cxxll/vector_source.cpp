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

#include <cxxll/vector_source.hpp>

#include <cstring>

using namespace cxxll;

vector_source::vector_source(const std::vector<unsigned char> *src)
  : source(src), position(0)
{
}

vector_source::~vector_source()
{
}

size_t
vector_source::read(unsigned char *buf, size_t len)
{
  if (position >= source->size()) {
    return 0;
  }
  size_t to_copy = std::min(len, source->size() - position);
  memcpy(buf, source->data() + position, to_copy);
  position += to_copy;
  return to_copy;
}

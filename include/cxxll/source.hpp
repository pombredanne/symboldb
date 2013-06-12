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

#include <cstddef>

namespace cxxll {

// Source of a sequence of bytes.
struct source {
  virtual ~source();

  // Returns 0 on end-of-stream.  Must throw an exception on error.
  virtual size_t read(unsigned char *, size_t) = 0;
};

// Read the required amount of bytes.  Throws eof_exception if not
// enough bytes are available.  May throw other exceptions, depending
// on the source.
void read_exactly(source &, unsigned char *, size_t);

} // namespace cxxll

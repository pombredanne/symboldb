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

#include <expat.h>

namespace cxxll {

struct expat_handle {
  XML_Parser raw;

  // Initializes raw by invoking XML_ParserCreate(NULL).
  // Throws std::bad_alloc on error.
  expat_handle();

  // Deallocates the raw pointer.  If you want to avoid this, set it
  // to NULL.
  ~expat_handle();

private:
  expat_handle(const expat_handle &); // not implemented
  expat_handle &operator=(const expat_handle &); // not implemented
};

} // namespace cxxll

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

#include "malloc_handle.hpp"

#include <cstdio>

namespace cxxll {

// Wrapper for a C FILE * handle.
class file_handle {
  FILE *raw;
public:
  file_handle();
  // Takes ownership.  Throws os_exception if FILE * is NULL.
  explicit file_handle(FILE *);
  // Opens a new file.  Throws os_exception on error.
  file_handle(const char *format, const char *flags);
  ~file_handle();

  // Closes the raw pointer (if it is not already NULL).
  void close();

  // Returns the raw pointer.
  FILE *get();

  // Releases ownership and returns the pointer.
  FILE *release();

  // Closes the current file, and takes ownership of the new pointer.
  // Throws os_exception on error (closing the new pointer).
  void reset(FILE *);

  // Reads a line from the file and possibly reallocates LINE,
  // updating LENGTH.  Returns false on EOF and throws os_exception on
  // error.
  bool getline(malloc_handle<char> &line, size_t &length);
};

inline
file_handle::file_handle()
  : raw(NULL)
{
}

inline FILE *
file_handle::get()
{
  return raw;
}

inline FILE *
file_handle::release()
{
  FILE *p = raw;
  raw = NULL;
  return p;
}

} // namespace cxxll

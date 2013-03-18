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

#include <dirent.h>

// Wrapper around DIR pointers.
class dir_handle {
  DIR *raw;
  dir_handle(const dir_handle &); // not implemented
  dir_handle &operator=(const dir_handle &); // not implemented
public:

  // Initialises the raw pointer to NULL.
  dir_handle()
    : raw(0)
  {
  }

  // Calls fdopendir(), which takes ownership of the descriptor.
  // Throws os_exception on error and closes the handle.
  // NOTE: This takes ownership of the descriptor, even on exceptions.
  explicit dir_handle(int);

  // Calls opendir().  Throws os_exception on error.
  explicit dir_handle(const char *);

  // Invokes closedir() if the raw pointer is not NULL.
  ~dir_handle();

  // Returns the DIR pointer and sets the raw pointer to NULL.
  DIR *release() throw();

  // Calls closedir() and sets the raw pointer to NULL.
  void close();

  // Calls readdir(), but skips over "." and "...  Throws os_exception
  // on error and returns NULL on end-of-directory.
  dirent *readdir();

  // Like readdir(), but does not skip over "." and "..".
  dirent *readdir_all();

  // Calls dirfd().
  int dirfd() const;
};

inline DIR *
dir_handle::release() throw()
{
  DIR *p = raw;
  raw = 0;
  return p;
}

inline int
dir_handle::dirfd() const
{
  return ::dirfd(raw);
}

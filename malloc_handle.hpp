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

#include <cstdlib>

// Helper class to ensure that free() is called on a malloc()ed
// pointer on scope exit.
template <class T>
class malloc_handle {
  malloc_handle(const malloc_handle &); // not implemented
  malloc_handle &operator=(const malloc_handle &); // not implemented
public:
  T *raw;
  
  malloc_handle()
    : raw(0)
  {
  }

  explicit malloc_handle(T *p)
    : raw(p)
  {
  }

  ~malloc_handle()
  {
    free(raw);
  }
};

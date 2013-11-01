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

namespace cxxll {

// Helper class to ensure that free() is called on a malloc()ed
// pointer on scope exit.
template <class T>
class malloc_handle {
  malloc_handle(const malloc_handle &) = delete;
  malloc_handle &operator=(const malloc_handle &) = delete;
  T *raw;
public:
  malloc_handle()
    : raw(nullptr)
  {
  }

  // Takes ownership of the raw pointer.
  explicit malloc_handle(T *p);

  // Frees the raw pointer.
  ~malloc_handle();

  // Returns the value of the raw pointer.
  T *get();

  // Returns the value of the raw pointer.
  const T *get() const;

  // Releases ownership of the raw pointer and returns it.
  T *release();

  // Changes the raw pointer (taking ownership) and frees the old
  // pointer if necessary.
  void reset(T *);
};

template <class T> inline
malloc_handle<T>::malloc_handle(T *p)
  : raw(p)
{
}

template <class T> inline
malloc_handle<T>::~malloc_handle()
{
  free(raw);
}

template <class T> inline T *
malloc_handle<T>::get()
{
  return raw;
}

template <class T> inline const T *
malloc_handle<T>::get() const
{
  return raw;
}

template <class T> inline T *
malloc_handle<T>::release()
{
  T *r = raw;
  raw = nullptr;
  return r;
}

template <class T> inline void
malloc_handle<T>::reset(T *p)
{
  free(raw);
  raw = p;
}

} // namespace cxxll

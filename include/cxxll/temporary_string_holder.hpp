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

class const_stringref;

// Class for holding a reference to a single string pointer.  This
// isued by const_stringref::c_str() to extend the life-time of the
// returned pointer.
class temporary_string_holder {
  friend class const_stringref;
  mutable char *ptr_;
  temporary_string_holder();
  ~temporary_string_holder();
  temporary_string_holder(const temporary_string_holder &) = delete;
  void operator=(const temporary_string_holder &) = delete;
};

inline
temporary_string_holder::temporary_string_holder()
  : ptr_(0)
{
}

inline
temporary_string_holder::~temporary_string_holder()
{
  free(ptr_);
}


} // namespace cxxll

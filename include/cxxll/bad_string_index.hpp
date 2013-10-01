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

#include <stdexcept>

namespace cxxll {

// This exception is thrown if const_stringref operations are used
// with an invalid index.
class bad_string_index : public std::out_of_range {
  size_t size_;
  size_t index_;
public:
  bad_string_index(size_t size, size_t index);
  ~bad_string_index() throw();

  // The size of the string reference.
  size_t size() const;

  // The invalid index which triggered this exception.
  size_t index() const;
};

inline size_t
bad_string_index::size() const
{
  return size_;
}

inline size_t
bad_string_index::index() const
{
  return index_;
}

} // namespace cxxll

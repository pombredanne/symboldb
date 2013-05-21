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

#include <string>

namespace cxxll {

class temporary_directory {
  std::string path_;
  temporary_directory(const temporary_directory &); // not implemented
  temporary_directory &operator=(const temporary_directory &); // not impl
public:
  // Creates a new temporary directory in an existing temporary
  // directory.  May throw os_exception.
  temporary_directory();

  // Creates a new temporary directory with the indicated path prefix.
  // May throw os_exception.
  explicit temporary_directory(const char *prefix);

  // Tries to delete the temporary directory.  Does not throw an
  // exception.
  ~temporary_directory();

  // Deletes the temporary directory.  May throw os_exception.
  void remove();

  // Returns the path to the temporary directory.
  const std::string &path() const;

  // Returns the full path name for a file in the temporary directory.
  std::string path(const char * name) const;
};

inline const std::string &
temporary_directory::path() const
{
  return path_;
}

} // namespace cxxll

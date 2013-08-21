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

#include <tr1/memory>
#include <string>
#include <vector>

namespace cxxll {

// Extractor for Python imports.  First call parse() with the Python
// program, then error() or imports() to obtain the list of imports.
// Member functions may throw os_exception in case of unexpected
// errors.
class python_analyzer {
  struct impl;
  std::tr1::shared_ptr<impl> impl_;
  python_analyzer(const python_analyzer &); // not implemented
  python_analyzer &operator=(const python_analyzer &); // not implemented
public:
  // Throws os_exception if no Python interpreter can be found.
  python_analyzer();
  ~python_analyzer();

  // Parses the Python source code SOURCE.  Returns true on success,
  // false on failure.
  bool parse(const std::vector<unsigned char> &source);

  // Returns true if the previous call to parse() was successful.
  bool good() const;

  // Returns a vector with the imports.  Relative imports contain one
  // or more leading '.'.
  const std::vector<std::string> &imports() const;

  // Returns a vector with the names of the extracted attribute
  // references.
  const std::vector<std::string> &attributes() const;

  // Returns a vector with the names of (nested) function definitions.
  const std::vector<std::string> &functions() const;

  // Returns the Python version which successfully parsed the source
  // code, or zero on error.
  unsigned version() const;

  // Returns a string with the error message, if any.
  const std::string &error_message() const;

  // Returns the line number of the error (starting at one), or zero
  // if there was no error.
  unsigned error_line() const;
};

} // namespace cxxll

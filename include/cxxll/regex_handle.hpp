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
#include <string>
#include <tr1/memory>

namespace cxxll {

// POSIX extended regular expression.
class regex_handle {
  struct impl;
  std::tr1::shared_ptr<impl> impl_;
public:
  explicit regex_handle(const char *);
  ~regex_handle();

  bool match(const char *subject) const;

  // Error during compilation of a regular expression.
  class error : public std::exception {
    std::string what_;
    int code_;
  public:
    error(const char *message, int code);
    ~error() throw();
    int code() const;
    const char *what() const throw();
  };
};

inline int
regex_handle::error::code() const
{
  return code_;
}

} // namespace cxxll

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

#include <rpm/rpmfi.h>

namespace cxxll {

// Minimal wrapper around rpmfi pointers.
class rpmfi_handle {
  rpmfi raw_;
  rpmfi_handle(const rpmfi_handle &); // not implemented
  rpmfi_handle &operator=(const rpmfi_handle &); // not implemented
public:
  // Throws rpm_parser_exception on failure.
  rpmfi_handle();
  explicit rpmfi_handle(Header);
  ~rpmfi_handle();

  // Returns the raw rpmfi handle.
  rpmfi get();

  // Frees the raw rpmfi handle.
  void close();

  // Switches this handle to a handle for the passed header.
  void reset_header(Header);

  // Invokes rpmfiNext() and returns true if there was another file
  // element.
  bool next();
};

inline
rpmfi_handle::rpmfi_handle()
  : raw_(NULL)
{
}

inline rpmfi
rpmfi_handle::get()
{
  return raw_;
}

inline bool
rpmfi_handle::next()
{
  return rpmfiNext(raw_) != -1;
}

} // namespace cxxll

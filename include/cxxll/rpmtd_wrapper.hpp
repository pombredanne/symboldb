/*
 * Copyright (C) 2012, 2013 Red Hat, Inc.
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

#include <rpm/rpmtd.h>

namespace cxxll {

// Simple wrapper around an rpmtd value.
struct rpmtd_wrapper {
  rpmtd raw;
 
  rpmtd_wrapper()
    : raw(rpmtdNew())
  {
  }

  // Frees also the pointed-to data.
  // If this is not what you want, call reset() first.
  ~rpmtd_wrapper();

  // Invokes rpmtdReset(raw) unless raw is NULL.
  void reset();

private:
  rpmtd_wrapper(const rpmtd_wrapper &) = delete;
  rpmtd_wrapper &operator=(const rpmtd_wrapper &) = delete;
};

} // namespace cxxll

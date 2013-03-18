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

#include "source.hpp"

// Source which reads from a POSIX file descriptor.  Does not take
// ownership of the descriptor.  Reading throws os_exception on error.
struct fd_source : source {
  int raw;

  // Initializes RAW to -1.
  fd_source();

  // Initializes RAW to the specified descriptor.
  fd_source(int);

  ~fd_source();

  size_t read(unsigned char *, size_t);
};

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

#include "sink.hpp"

// Sink which writes to a POSIX file descriptor.  Does not take
// ownership of the descriptor.  Writing throws os_exception on error.
struct fd_sink : sink {
  int raw;

  // Initializes RAW to -1.
  fd_sink();

  // Initializes RAW to the specified descriptor.
  explicit fd_sink(int);

  ~fd_sink();

  void write(const unsigned char *, size_t);
};

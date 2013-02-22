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

// POSIX file descriptor which is closed on scope exit.
struct fd_handle {
  int raw;

  fd_handle()
    : raw(-1)
  {
  }

  // Opens the file with the indicated flags.  Throws os_exception on
  // error.
  void open(const char *path, int flags);

  // Opens the file with the indicated flags.  Throws os_exception on
  // error.
  void open(const char *path, int flags, unsigned mode);

  // Opens the file for reading.  Throws os_exception on error.
  void open_read_only(const char *path);

  // Opens this descriptor relative to the passed-in descriptor.
  // Throws os_exception on error.
  void openat(int fd, const char *path, int flags);
  void openat(int fd, const char *path, int flags, unsigned mode);

  // Closes RAW if it is not negative.
  ~fd_handle();

  // Returns the current value of RAW and sets it to -1, effectively
  // releasing ownership.
  int release();

private:
  fd_handle(const fd_handle &); // not implemented
  fd_handle &operator=(const fd_handle &); // not implemented
};

inline int
fd_handle::release()
{
  int fd = raw;
  raw = -1;
  return fd;
}

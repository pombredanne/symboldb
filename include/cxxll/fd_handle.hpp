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

#include <cstddef>
#include <string>

namespace cxxll {

// POSIX file descriptor which is closed on scope exit.
class fd_handle {
  int raw;
  fd_handle(const fd_handle &) = delete;
  fd_handle &operator=(const fd_handle &) = delete;
public:
  // Sets the file handle to -1.
  fd_handle();

  // Takes ownership of the file descriptor.
  explicit fd_handle(int);

  // Closes the file descriptor if it is not negative.
  ~fd_handle();

  // Swaps the descriptors.
  void swap(fd_handle &) throw();

  // Returns the file descriptor.
  int get() throw();

  // Replaces the file descriptor with the specified one, taking
  // ownership.  If closing the old descriptor results an error, the
  // new descriptor is closed as well.
  void reset(int);

  // Opens the file with the indicated flags.  Throws os_exception on
  // error.
  void open(const char *path, int flags);

  // Opens the file with the indicated flags.  Throws os_exception on
  // error.
  void open(const char *path, int flags, unsigned mode);

  // Opens the file for reading.  Throws os_exception on error.
  void open_read_only(const char *path);

  // Opens PATH as a directory.  Throws os_exception on error.
  void open_directory(const char *path);

  // Opens this descriptor relative to the passed-in descriptor.
  // Throws os_exception on error.
  void openat(int fd, const char *path, int flags);
  void openat(int fd, const char *path, int flags, unsigned mode);

  // Calls ::mkostemp(char *, O_CLOEXEC).  Appends "XXXXXX" to PREFIX
  // and returns the actual file name.
  std::string mkstemp(const char *prefix);

  // Assigns this fd_handle a duplicate of the descriptor.  The new
  // descriptor has the O_CLOEXEC flag set.  Throws os_exception on
  // error.
  void dup(int fd);

  // Changes or returns the status of the FD_CLOEXEC flag.
  void close_on_exec(bool);
  bool close_on_exec() const;

  // Returns the current value of the file descriptor and sets it to
  // -1, effectively releasing ownership.
  int release() throw();

  // Closes the descriptor and sets the file descriptor to -1,
  // throwing os_exception on error.
  void close();

  // Calls read(int, void *, size_t).  Throws os_exception on error.
  size_t read(void *, size_t);

  // Closes the descriptor if open, ignoring error return values.
  void close_nothrow() throw();

  // Calls fsync(int).  Throws os_exception on error.
  void fsync();

  // Calls mkdirat(int, const char *, mode_t).
  void mkdirat(const char *name, unsigned mode);

  // Calls unlinkat(int, const char *, int).  Throws os_exception on error.
  void unlinkat(const char *pathname, int flags);
};

inline
fd_handle::fd_handle()
  : raw(-1)
{
}

inline
fd_handle::fd_handle(int fd)
  : raw(fd)
{
}

inline void
fd_handle::swap(fd_handle &other) throw()
{
  int tmp = other.raw;
  other.raw = raw;
  raw = tmp;
}

inline int
fd_handle::get() throw()
{
  return raw;
}

inline int
fd_handle::release() throw()
{
  int fd = raw;
  raw = -1;
  return fd;
}

// Throws os_exception on error.
void renameat(fd_handle &from, const char *from_name,
	      fd_handle &to, const char *to_name);

} // namespace cxxll

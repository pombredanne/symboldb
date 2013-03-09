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

#include "fd_handle.hpp"
#include "os_exception.hpp"

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

fd_handle::~fd_handle()
{
  close_nothrow();
}

void
fd_handle::reset(int fd)
{
  if (raw >= 0 && ::close(raw) < 0) {
    int err = errno;
    if (fd >= 0) {
      ::close(fd);
    }
    throw os_exception(err).function(::close).fd(raw);
  }
  raw = fd;
}

void
fd_handle::open(const char *path, int flags)
{
  int ret = ::open(path, flags);
  if (ret < 0) {
    throw os_exception().function(::open).path(path);
  }
  if (raw >= 0) {
    ::close(raw);
  }
  raw = ret;
}

void
fd_handle::open(const char *path, int flags, unsigned mode)
{
  int ret = ::open(path, flags, mode);
  if (ret < 0) {
    throw os_exception().function(::open).path(path);
  }
  if (raw >= 0) {
    ::close(raw);
  }
  raw = ret;
}

void
fd_handle::openat(int fd, const char *path, int flags, unsigned mode)
{
  int ret = ::openat(fd, path, flags, mode);
  if (ret < 0) {
    throw os_exception().function(::openat).fd(fd).path2(path).defaults();
  }
  if (raw >= 0) {
    ::close(raw);
  }
  raw = ret;
}

void
fd_handle::openat(int fd, const char *path, int flags)
{
  int ret = ::openat(fd, path, flags);
  if (ret < 0) {
    throw os_exception().function(::openat).fd(fd).path2(path).defaults();
  }
  if (raw >= 0) {
    ::close(raw);
  }
  raw = ret;
}

void
fd_handle::open_read_only(const char *path)
{
  open(path, O_RDONLY | O_CLOEXEC);
}

void
fd_handle::open_directory(const char *path)
{
  open(path, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
}

void
fd_handle::close_on_exec(bool flag)
{
  int flags = fcntl(raw, F_GETFD, 0);
  if (flags == -1) {
    throw os_exception().function(fcntl).message("F_GETFD");
  }
  if (flag) {
    flags |= FD_CLOEXEC;
  } else {
    flags &= ~FD_CLOEXEC;
  }
  if (fcntl(raw, F_SETFD, flags) == -1) {
    throw os_exception().function(fcntl).message("F_SETFD");
  }
}

bool
fd_handle::close_on_exec() const
{
  int flags = fcntl(raw, F_GETFD, 0);
  if (flags == -1) {
    throw os_exception().function(fcntl).message("F_GETFD");
  }
  return (flags & FD_CLOEXEC) != 0;
}

void
fd_handle::close()
{
  if (raw < 0) {
    return;
  }
  int fd = raw;
  // Linux close() always closes the file descriptor, even on error.
  raw = -1;
  if (::close(fd) != 0) {
    throw os_exception().function(::close).fd(fd); // no defaults
  }
}

void
fd_handle::close_nothrow() throw()
{
  if (raw >= 0) {
    ::close(raw);
    raw = -1;
  }
}

void
fd_handle::fsync()
{
  if (::fsync(raw) < 0) {
    throw os_exception().function(::fsync).fd(raw).defaults();
  }
}

size_t
fd_handle::read(void *buffer, size_t length)
{
  ssize_t result = ::read(raw, buffer, length);
  if (result < 0) {
    throw os_exception().function(::read).fd(raw).count(length).defaults();
  }
  return result;
}

void
fd_handle::mkdirat(const char *pathname, unsigned mode)
{
  if (::mkdirat(raw, pathname, mode) < 0) {
    throw os_exception().function(::mkdirat).fd(raw).path2(pathname)
      .defaults();
  }
}

void
fd_handle::unlinkat(const char *pathname, int flags)
{
  if (::unlinkat(raw, pathname, flags) < 0) {
    throw os_exception().function(::unlinkat).fd(raw).path2(pathname).defaults();
  }
}

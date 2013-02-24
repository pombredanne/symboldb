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

#include <fcntl.h>
#include <unistd.h>

fd_handle::~fd_handle()
{
  close_nothrow();
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
fd_handle::close_nothrow() throw()
{
  if (raw >= 0) {
    close(raw);
    raw = -1;
  }
}

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
  if (raw >= 0) {
    close(raw);
  }
}

void
fd_handle::open(const char *path, int flags)
{
  int ret = ::open(path, flags);
  if (ret < 0) {
    throw os_exception().function(::open).path(path);
  }
  ::close(raw);
  raw = ret;
}

void
fd_handle::open_read_only(const char *path)
{
  open(path, O_RDONLY | O_CLOEXEC);
}

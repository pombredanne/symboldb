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

#include <cxxll/fd_source.hpp>
#include <cxxll/os_exception.hpp>

#include <errno.h>
#include <unistd.h>

using namespace cxxll;

fd_source::fd_source()
  : raw(-1)
{
}

fd_source::fd_source(int d)
  : raw(d)
{
}

fd_source::~fd_source()
{
}

size_t
fd_source::read(unsigned char *buf, size_t len)
{
  ssize_t ret = ::read(raw, buf, len);
  if (ret < 0) {
    throw os_exception().fd(raw).count(len).function(::read).defaults();
  }
  return ret;
}

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

#include <cxxll/fd_sink.hpp>
#include <cxxll/os_exception.hpp>

#include <errno.h>
#include <unistd.h>

using namespace cxxll;

fd_sink::fd_sink()
  : raw(-1)
{
}

fd_sink::fd_sink(int d)
  : raw(d)
{
}

fd_sink::~fd_sink()
{
}

void
fd_sink::
write(const_stringref buf)
{
  while (!buf.empty()) {
    ssize_t ret = ::write(raw, buf.data(), buf.size());
    if (ret == 0) {
      ret = -1;
      errno = ENOSPC;
    }
    if (ret < 0) {
      throw os_exception().fd(raw).count(buf.size())
	.function(::write).defaults();
    }
    buf += ret;
  }
}

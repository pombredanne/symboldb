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

#include "sink.hpp"
#include "source.hpp"
#include "source_sink.hpp"

#include <cassert>

unsigned long long
copy_source_to_sink(source &src, sink &dst)
{
  unsigned long long count = 0;
  unsigned char buf[8192];
  while (true) {
    size_t ret = src.read(buf, sizeof(buf));
    assert(ret <= sizeof(buf));
    if (ret == 0) {
      break;
    }
    dst.write(buf, ret);
    count += ret;
  }
  return count;
}
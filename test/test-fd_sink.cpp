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

#include "test.hpp"
#include <cxxll/fd_handle.hpp>
#include <cxxll/fd_sink.hpp>
#include <cxxll/os_exception.hpp>
#include <cxxll/string_support.hpp>

#include <fcntl.h>

using namespace cxxll;

static void
test(void)
{
  {
    fd_handle h;
    h.open("/dev/full", O_WRONLY);
    fd_sink s(h.get());
    unsigned char buf[3] = {65, 66, 67};
    try {
      s.write(buf, sizeof(buf));
    } catch (os_exception &e) {
      CHECK(starts_with(e.what(), "function=write["));
      CHECK(ends_with(e.what(),
		      "error=ENOSPC fd=3 path=/dev/full"
		      " offset=0 length=0 count=3"));
    }
  }
}

static test_register t("fd_sink", test);

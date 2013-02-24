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
#include "fd_handle.hpp"
#include "os_exception.hpp"
#include "string_support.hpp"

#include <errno.h>
#include <fcntl.h>

static void
test(void)
{
  {
    int fd;
    {
      fd_handle h;
      h.open_read_only("/dev/null");
      fd = h.raw;
      CHECK(h.release() == fd);
      CHECK(h.raw == -1);
      h.raw = fd;
    }
    CHECK(fd > 2);
    {
      fd_handle h;
      h.open_read_only("/dev/null");
      CHECK(h.raw == fd);
    }
    {
      fd_handle h;
      h.open_read_only("/dev/null");
      CHECK(h.raw == fd);
      h.open_read_only("/dev/null");
      CHECK(h.raw == fd + 1);
      h.close_nothrow();
      CHECK(h.raw == -1);
      h.open_read_only("/dev/null");
      CHECK(h.raw == fd);
      h.open_read_only("/dev/null");
      CHECK(h.raw == fd + 1);
    }
  }

  {
    fd_handle dir;
    dir.open("/dev", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    CHECK(dir.raw == 3);
    {
      fd_handle h;
      h.openat(dir.raw, "null", O_RDONLY | O_CLOEXEC);
      CHECK(h.raw == 4);
    }

    try {
      fd_handle h;
      h.openat(dir.raw, "#does-not-exist#", O_RDONLY);
    } catch (os_exception &e) {
      CHECK(starts_with(e.function_name(), "openat["));
      CHECK(e.error_code() == ENOENT);
      CHECK(e.fd() == 3);
      COMPARE_STRING(e.path(), "/dev");
      COMPARE_STRING(e.path2(), "#does-not-exist#");
    }
  }


  try {
    fd_handle h;
    h.open_read_only("#does-not-exist#");
  } catch (os_exception &e) {
    CHECK(starts_with(e.what(), "function=open["));
    CHECK(ends_with(e.what(),
		    "] error=ENOENT path=#does-not-exist#"));
  }
}

static test_register t("fd_handle", test);

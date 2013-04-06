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
#include <cxxll/os_exception.hpp>
#include <cxxll/string_support.hpp>
#include <cxxll/os.hpp>

#include <errno.h>
#include <fcntl.h>

using namespace cxxll;

static void
test(void)
{
  {
    int fd;
    {
      fd_handle h;
      h.open_read_only("/dev/null");
      fd = h.get();
      CHECK(h.release() == fd);
      CHECK(h.get() == -1);
      h.reset(fd);
    }
    CHECK(fd > 2);
    {
      fd_handle h;
      h.open_read_only("/dev/null");
      CHECK(h.get() == fd);
    }
    {
      fd_handle h;
      h.open_read_only("/dev/null");
      CHECK(h.get() == fd);
      h.open_read_only("/dev/null");
      CHECK(h.get() == fd + 1);
      h.close_nothrow();
      CHECK(h.get() == -1);
      h.open_read_only("/dev/null");
      CHECK(h.get() == fd);
      h.open_read_only("/dev/null");
      CHECK(h.get() == fd + 1);
      CHECK(h.close_on_exec());
      h.close_on_exec(false);
      CHECK(!h.close_on_exec());
      h.close_on_exec(true);
      CHECK(h.close_on_exec());
      h.close();
      CHECK(h.get() == -1);
      h.close();
      h.reset(99999);
      try {
	h.close();
	CHECK(false);
      } catch (os_exception &e) {
	CHECK(e.error_code() == EBADF);
	CHECK(e.fd() == 99999);
	CHECK(starts_with(e.function_name(), "close["));
      }
    }
  }

  {
    fd_handle dir;
    dir.open("/dev", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    CHECK(dir.get() == 3);
    {
      fd_handle h;
      h.openat(dir.get(), "null", O_RDONLY | O_CLOEXEC);
      CHECK(h.get() == 4);
    }

    try {
      fd_handle h;
      h.openat(dir.get(), "#does-not-exist#", O_RDONLY);
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

  {
    fd_handle dir;
    dir.open_directory("/dev");
    fd_handle dir2;
    dir2.dup(dir.get());
    CHECK(dir2.close_on_exec());
    CHECK(dir2.get() == dir.get() + 1);
    dir.swap(dir2);
    CHECK(dir2.get() == dir.get() - 1);
    fd_handle null;
    null.openat(dir.get(), "null", O_RDONLY);
    try {
      dir.open_directory("/dev/null");
      CHECK(false);
    } catch (os_exception &e) {
      CHECK(e.error_code() == ENOTDIR);
    }
  }
  {
    std::string root(make_temporary_directory("/tmp/test-fd_handle-"));
    try {
      fd_handle fd;
      fd.open_directory(root.c_str());
      fd.mkdirat("first", 0777);
      fd.mkdirat("second", 0777);
      fd_handle first;
      first.openat(fd.get(), "first", O_RDONLY| O_DIRECTORY | O_CLOEXEC);
      fd_handle second;
      second.openat(fd.get(), "second", O_RDONLY| O_DIRECTORY | O_CLOEXEC);
      {
	fd_handle file;
	file.openat(first.get(), "abc", O_WRONLY | O_CREAT | O_CLOEXEC, 0777);
      }
      CHECK(path_exists((root + "/first/abc").c_str()));
      renameat(first, "abc", second, "xyzt");
      CHECK(!path_exists((root + "/first/abc").c_str()));
      CHECK(path_exists((root + "/second/xyzt").c_str()));
    } catch (...) {
      remove_directory_tree(root.c_str());
      throw;
    }
    remove_directory_tree(root.c_str());
  }
  {
    fd_handle fd;
    std::string prefix("/tmp/test-fd_handle-");
    std::string path(fd.mkstemp(prefix.c_str()));
    CHECK(path.size() > prefix.size());
    std::string pathpfx(path);
    pathpfx.resize(prefix.size());
    COMPARE_STRING(pathpfx, prefix);
    CHECK(fd.close_on_exec());
    CHECK(path_exists(path.c_str()));
    fd.unlinkat(path.c_str(), 0);
    CHECK(!path_exists(path.c_str()));
  }
}

static test_register t("fd_handle", test);

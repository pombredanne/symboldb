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

#include "os.hpp"
#include "fd_handle.hpp"
#include "os_exception.hpp"
#include "string_support.hpp"

#include "test.hpp"

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

static void
test(void)
{
  int fd_baseline;
  {
    fd_handle h;
    h.open_read_only("/dev/null");
    fd_baseline = h.get();
  }

  CHECK(is_directory("."));
  CHECK(is_directory(".."));
  CHECK(is_directory("/"));
  CHECK(is_directory("/tmp"));
  CHECK(!is_directory(""));
  CHECK(!is_directory("#does-not-exists#"));
  CHECK(!is_directory("/tmp/#does-not-exists#"));

  CHECK(current_directory() != ".");
  COMPARE_STRING(current_directory(), realpath("."));

  {
    std::string dir(make_temporary_directory("/tmp/test-os-"));
    CHECK(is_directory(dir.c_str()));
    struct stat64 st;
    CHECK(stat64(dir.c_str(), &st) == 0);
    CHECK((st.st_mode & S_IRWXU) == 0700);
    CHECK(make_directory_hierarchy((dir + "/first/second").c_str(), 0700));
    CHECK(stat64((dir + "/first/second").c_str(), &st) == 0);
    CHECK((st.st_mode & S_IRWXU) == 0700);
    CHECK(stat64((dir + "/first").c_str(), &st) == 0);
    CHECK((st.st_mode & S_IRWXU) == 0700);
    CHECK(rmdir((dir + "/first/second").c_str()) == 0);
    CHECK(rmdir((dir + "/first").c_str()) == 0);
    CHECK(rmdir(dir.c_str()) == 0);
    CHECK(!is_directory(dir.c_str()));
  }

  {
    std::string dir(make_temporary_directory("/tmp/test-os-"));
    CHECK(is_directory(dir.c_str()));
    CHECK(make_directory_hierarchy((dir + "/first/second").c_str(), 0700));
    {
      fd_handle h;
      h.open((dir + "/some-file").c_str(), O_WRONLY | O_CREAT | O_CLOEXEC, 0600);
    }
    CHECK(access((dir + "/some-file").c_str(), R_OK) == 0);
    remove_directory_tree(dir.c_str());
    CHECK(!is_directory(dir.c_str()));
  }

  COMPARE_STRING(realpath("/"), "/");
  try {
    realpath("#does-not-exist#");
  } catch (os_exception &e) {
    CHECK(starts_with(e.what(), "function=realpath["));
    CHECK(ends_with(e.what(),
		    "] error=ENOENT path=#does-not-exist#"));
  }

  try {
    readlink(".");
  } catch (os_exception &e) {
    CHECK(starts_with(e.what(), "function=readlink["));
    CHECK(ends_with(e.what(), "] error=EINVAL path=."));
  }

  CHECK(error_string_or_null(0) == NULL);
  CHECK(error_string_or_null(-1) == NULL);
  COMPARE_STRING(error_string(0), "ERROR(0)");
  COMPARE_STRING(error_string(-1), "ERROR(-1)");
  COMPARE_STRING(error_string(EINVAL), "EINVAL");
  COMPARE_STRING(error_string_or_null(EINVAL), "EINVAL");
  COMPARE_STRING(error_string(ERANGE), "ERANGE");
  COMPARE_STRING(error_string_or_null(ERANGE), "ERANGE");

  // Check for file descriptor leaks.
  {
    fd_handle h;
    h.open_read_only("/dev/null");
    CHECK(h.get() == fd_baseline);
  }
}

static test_register t("os", test);

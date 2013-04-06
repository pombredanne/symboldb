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

#include <cxxll/temporary_directory.hpp>
#include <cxxll/os.hpp>
#include <cxxll/fd_handle.hpp>
#include <cxxll/string_support.hpp>

#include <fcntl.h>

#include "test.hpp"

using namespace cxxll;

static void
test(void)
{
  std::string path;
  {
    temporary_directory tmp;
    path = tmp.path();
    CHECK(is_directory(path.c_str()));
    CHECK(ends_with(tmp.path("file").c_str(), "/file"));
    fd_handle fd;
    fd.open(tmp.path("file").c_str(), O_WRONLY | O_CREAT | O_EXCL, 0666);
    CHECK(path_exists(tmp.path("file").c_str()));
  }
  CHECK(!is_directory(path.c_str()));

  {
    temporary_directory tmp;
    path = tmp.path();
    CHECK(is_directory(path.c_str()));
    tmp.remove();
    CHECK(!is_directory(path.c_str()));
    COMPARE_STRING(tmp.path(), "");
  }
}

static test_register t("temporary_directory", test);

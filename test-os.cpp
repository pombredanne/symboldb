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
#include "test.hpp"
#include "os_exception.hpp"
#include "string_support.hpp"

static void
test(void)
{
  CHECK(is_directory("."));
  CHECK(is_directory(".."));
  CHECK(is_directory("/"));
  CHECK(is_directory("/tmp"));
  CHECK(!is_directory(""));
  CHECK(!is_directory("#does-not-exists#"));
  CHECK(!is_directory("/tmp/#does-not-exists#"));

  CHECK(current_directory() != ".");
  COMPARE_STRING(current_directory(), realpath("."));

  COMPARE_STRING(realpath("/"), "/");
  try {
    realpath("#does-not-exist#");
  } catch (os_exception &e) {
    CHECK(starts_with(e.what(), "function=realpath["));
    CHECK(ends_with(e.what(),
		    "] error=\"No such file or directory\""
		    " path=#does-not-exist#"));
  }

  try {
    readlink(".");
  } catch (os_exception &e) {
    CHECK(starts_with(e.what(), "function=readlink["));
    CHECK(ends_with(e.what(), "] error=\"Invalid argument\" path=."));
  }
}

static test_register t("os", test);

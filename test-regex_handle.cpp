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

#include "regex_handle.hpp"
#include "test.hpp"

#include <regex.h>

static void
test()
{
  regex_handle rx("^foo$");
  CHECK(rx.match("foo"));
  CHECK(!rx.match("foo\n"));
  try {
    regex_handle("(foo");
    CHECK(false);
  } catch (regex_handle::error &e) {
    CHECK(e.code() == REG_EPAREN);
  }
}

static test_register t("regex", test);

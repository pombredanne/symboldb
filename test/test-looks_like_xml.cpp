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

#include <cxxll/looks_like_xml.hpp>

#include "test.hpp"

using namespace cxxll;

namespace {
  template <size_t N>
  bool
  check(const char (&buf)[N])
  {
    return looks_like_xml(buf, buf + N - 1);
  }
}

static void
test()
{
  CHECK(!check(""));
  CHECK(check("<"));
  CHECK(check(" <"));
  CHECK(check("\n<"));
  CHECK(check("\t<"));
  CHECK(!check("\000<"));
  CHECK(!check("\177<"));
  CHECK(!check("a<"));
  CHECK(!check("\xEF"));
  CHECK(!check("\xEF\xBB"));
  CHECK(!check("\xEF\xBB\xBF"));
  CHECK(check("\xEF\xBB\xBF<"));
  CHECK(check(" \xEF\xBB\xBF <"));
}

static test_register t("looks_like_xml", test);

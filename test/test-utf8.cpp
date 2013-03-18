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

#include <cxxll/utf8.hpp>
#include "test.hpp"

static void
test()
{
  CHECK(is_valid_utf8(""));
  CHECK(is_valid_utf8("test"));
  CHECK(is_valid_utf8("Diese \303\234bung macht \303\204rger."));

  for (unsigned ch = 0; ch <= 127; ++ch) {
    CHECK(is_valid_utf8(std::string(1U, (char)ch)));
  }

  for (unsigned ch = 128; ch <= 255; ++ch) {
    CHECK(!is_valid_utf8(std::string(1U, (char)ch)));
    std::string s;
    for (unsigned ch2 = 0; ch2 <= 127; ++ch2) {
      s.clear();
      s += (char)ch;
      s += (char)ch2;
      CHECK(!is_valid_utf8(s));
      s.clear();
      s += (char)ch2;
      s += (char)ch;
      CHECK(!is_valid_utf8(s));
    }
  }

  COMPARE_STRING(latin1_to_utf8(""), "");
  COMPARE_STRING(latin1_to_utf8("test"), "test");
  COMPARE_STRING(latin1_to_utf8("\200"), "\302\200");
  COMPARE_STRING(latin1_to_utf8("\377"), "\303\277");
  COMPARE_STRING(latin1_to_utf8("Diese \334bung macht \304rger."),
		 "Diese \303\234bung macht \303\204rger.");
}

static test_register t("utf8", test);

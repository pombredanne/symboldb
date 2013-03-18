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

#include <cxxll/string_source.hpp>
#include "test.hpp"

static void
test()
{
  {
    string_source src("");
    unsigned char buf[5];
    CHECK(src.read(buf, 0) == 0);
    CHECK(src.read(buf, sizeof(buf)) == 0);
  }
  {
    string_source src("abc");
    unsigned char buf[5] = "xyzt";
    CHECK(src.read(buf, 0) == 0);
    COMPARE_STRING(std::string(buf, buf + sizeof(buf)),
		   std::string("xyzt", 5));
    CHECK(src.read(buf, sizeof(buf)) == 3);
    COMPARE_STRING(std::string(buf, buf + sizeof(buf)),
		   std::string("abct", 5));
    ++buf[0];
    CHECK(src.read(buf, sizeof(buf)) == 0);
    COMPARE_STRING(std::string(buf, buf + sizeof(buf)),
		   std::string("bbct", 5));
  }
  {
    string_source src("abcde");
    unsigned char buf[5] = "xyzt";
    CHECK(src.read(buf, 0) == 0);
    COMPARE_STRING(std::string(buf, buf + sizeof(buf)),
		   std::string("xyzt", 5));
    CHECK(src.read(buf, 3) == 3);
    COMPARE_STRING(std::string(buf, buf + sizeof(buf)),
		   std::string("abct", 5));
    CHECK(src.read(buf, 3) == 2);
    COMPARE_STRING(std::string(buf, buf + sizeof(buf)),
		   std::string("dect", 5));
    ++buf[0];
    CHECK(src.read(buf, 3) == 0);
    COMPARE_STRING(std::string(buf, buf + sizeof(buf)),
		   std::string("eect", 5));
  }
}

static test_register t("string_source", test);

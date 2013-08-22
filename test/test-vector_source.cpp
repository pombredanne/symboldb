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

#include <cxxll/vector_source.hpp>
#include <cxxll/eof_exception.hpp>
#include "test.hpp"

using namespace cxxll;

static void
test()
{
  {
    std::vector<unsigned char> vec;
    vector_source src(&vec);
    unsigned char buf[5];
    CHECK(src.read(buf, 0) == 0);
    CHECK(src.read(buf, sizeof(buf)) == 0);
  }
  {
    std::vector<unsigned char> vec;
    vec.push_back('a');
    vec.push_back('b');
    vec.push_back('c');
    vector_source src(&vec);
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
    std::vector<unsigned char> vec;
    vec.push_back('a');
    vec.push_back('b');
    vec.push_back('c');
    vec.push_back('d');
    vec.push_back('e');
    vector_source src(&vec);
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
  {
    std::vector<unsigned char> vec;
    vec.push_back('a');
    vec.push_back('b');
    vec.push_back('c');
    vec.push_back('d');
    vec.push_back('e');
    vector_source src(&vec);
    unsigned char buf[5] = "xyzt";
    read_exactly(src, buf, 3);
    COMPARE_STRING(std::string(buf, buf + sizeof(buf)),
		   std::string("abct", 5));
    try {
      read_exactly(src, buf, 3);
      CHECK(false);
    } catch (eof_exception &) {
      COMPARE_STRING(std::string(buf, buf + sizeof(buf)),
		     std::string("dect", 5));
    }
  }
}

static test_register t("vector_source", test);

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

#include <cxxll/read_file.hpp>
#include <cxxll/hash.hpp>
#include <cxxll/base16.hpp>

#include "test.hpp"

using namespace cxxll;

static void
test()
{
  static const char *hashhex =
    "a54b320846cd4d41b6aa1067a14e5ed560b41826e844297d00e17a3c5abb88f0";

  {
    std::vector<unsigned char> vec;
    read_file("/dev/null", vec);
    CHECK(vec.empty());
    vec.push_back('A');
    read_file("/dev/null", vec);
    CHECK(vec.size() == 1);
    CHECK(vec.at(0) == 'A');
    read_file("test/data/sysvinit-2.88-9.dsf.fc18.src.rpm", vec);
    CHECK(vec.size() > 1);
    CHECK(vec.at(0) == 'A');
    vec.erase(vec.begin(), vec.begin() + 1);
    std::vector<unsigned char> digest
      (hash(hash_sink::sha256, vec));
    COMPARE_STRING(hashhex, base16_encode(digest.begin(), digest.end()));
  }

  {
    std::vector<unsigned char> vec;
    read_file("test/data/sysvinit-2.88-9.dsf.fc18.src.rpm", vec);
    std::vector<unsigned char> digest
      (hash(hash_sink::sha256, vec));
    COMPARE_STRING(hashhex, base16_encode(digest.begin(), digest.end()));
  }
}

static test_register t("read_file", test);

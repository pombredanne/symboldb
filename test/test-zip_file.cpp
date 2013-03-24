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

#include <cxxll/zip_file.hpp>
#include <cxxll/read_file.hpp>

#include "test.hpp"

static void
test()
{
  using namespace cxxll;
  {
    std::vector<unsigned char> buffer;
    read_file("test/data/test.zip", buffer);
    zip_file zip(&buffer);
    CHECK(zip.next());
    COMPARE_STRING(zip.name(), "data1");
    std::vector<unsigned char> data;
    zip.data(data);
    COMPARE_STRING("data file 1 (not compressed)\n",
		   std::string(data.begin(), data.end()));
    CHECK(zip.next());
    COMPARE_STRING(zip.name(), "data2");
    data.clear();
    zip.data(data);
    COMPARE_STRING("data file 2 (this should be compressed)\n"
		   "data file 2 (this should be compressed)\n",
		   std::string(data.begin(), data.end()));
    CHECK(!zip.next());
  }
}

static test_register t("zip_file", test);

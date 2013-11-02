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

#include <cxxll/read_lines.hpp>
#include <cxxll/read_file.hpp>
#include <cxxll/os_exception.hpp>

#include "test.hpp"

using namespace cxxll;

static void
test()
{
  std::vector<std::string> lines;

  lines.push_back("line 0");
  read_lines("/dev/null", lines);
  COMPARE_NUMBER(lines.size(), 1U);
  COMPARE_STRING(lines.at(0), "line 0");

  try {
    read_lines("test/data/file_handle_getline.txt", lines);
    CHECK(false);
  } catch (os_exception &e) {
    COMPARE_STRING(e.path(), "test/data/file_handle_getline.txt");
    COMPARE_STRING(e.message(), "missing line feed at end of file");
  }

  {
    std::vector<unsigned char> ref;
    read_file("test/test-read_lines.cpp", ref);
    std::vector<unsigned char> bytes;
    lines.clear();
    read_lines("test/test-read_lines.cpp", lines);
    COMPARE_STRING(lines.at(0), "/*");
    COMPARE_STRING(lines.back(),
		   "static test_register t(\"read_lines\", test);");
    for (const std::string &line : lines) {
      bytes.insert(bytes.end(), line.begin(), line.end());
      bytes.push_back('\n');
    }
    COMPARE_NUMBER(ref.size(), bytes.size());
    CHECK(ref == bytes);
  }
}

static test_register t("read_lines", test);

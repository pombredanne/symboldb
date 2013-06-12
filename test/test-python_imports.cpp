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

#include <cxxll/python_imports.hpp>
#include <cxxll/read_file.hpp>

#include "test.hpp"

using namespace cxxll;

static const char *const imports[] = {
  "direct",
  "direct1",
  "direct2",
  "direct3",
  "direct4",
  "direct5",
  "direct.nested",
  "from1.id1",
  "from2.id3",
  "from2.id4",
  "from3.id5",
  "from3.id6",
  "from4.nested1.id7",
  "all1.*",
  ".relative1",
  ".relative.nested",
  ".relative2.*",
  ".relative.nested.id8",
  ".relative.nested.id9",
  "..relative3",
  "..relative4.id10",
  NULL
};

static void
test_one(python_imports &pi, const char *path)
{
  std::vector<unsigned char> src;
  read_file(path, src);
  CHECK(pi.parse(src));
  CHECK(pi.good());
  COMPARE_STRING(pi.error_message(), "");
  const char*const *p;
  for (p = imports; *p; ++p) {
    COMPARE_STRING(*p, pi.imports().at(p - imports));
  }
  size_t count = p - imports;
  COMPARE_NUMBER(pi.imports().size(), count);
}

static void
test_byte(unsigned char byte, const char *msg)
{
  std::vector<unsigned char> src;
  src.push_back('\n');
  src.push_back(byte);
  python_imports pi;
  CHECK(!pi.parse(src));
  COMPARE_STRING(pi.error_message(), msg);
  COMPARE_NUMBER(pi.error_line(), 2U);
  test_one(pi, "test/data/imports.py");
}

static void
test()
{
  {
    python_imports pi;
    CHECK(!pi.good());
    COMPARE_STRING(pi.error_message(), "");
    COMPARE_NUMBER(pi.version(), 0U);
    test_one(pi, "test/data/imports.py");
    COMPARE_NUMBER(pi.version(), 2U);
    test_one(pi, "test/data/imports3.py");
    COMPARE_NUMBER(pi.version(), 3U);
    test_one(pi, "test/data/imports.py");
    COMPARE_NUMBER(pi.version(), 2U);
  }
  {
    python_imports pi;
    CHECK(!pi.good());
    COMPARE_STRING(pi.error_message(), "");
    COMPARE_NUMBER(pi.version(), 0U);
    test_one(pi, "test/data/imports3.py");
    COMPARE_NUMBER(pi.version(), 3U);
    test_one(pi, "test/data/imports.py");
    COMPARE_NUMBER(pi.version(), 2U);
    test_one(pi, "test/data/imports3.py");
    COMPARE_NUMBER(pi.version(), 3U);
  }
  test_byte(0, "source code contains NUL character");
  test_byte('=', "invalid syntax");
}

static test_register t("python_imports", test);


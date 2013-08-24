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

#include <cxxll/pg_split_statement.hpp>

#include "test.hpp"

using namespace cxxll;

static void
test()
{
  std::vector<std::string> result;
  pg_split_statement
    ("-- Comment\n" "\n" "-- Comment 2;\n" "SELECT 1;\n"
    "--;Comment 3\n" "SELECT 2; "
    "SELECT 3;\n"
    "-- Comment ';'\n" "SELECT '';"
    "SELECT ';';"
    "SELECT 'abc';"
    "SELECT ''';';", result);
  static const char *const statements[] = {
    "SELECT 1;",
    "SELECT 2;",
    "SELECT 3;",
    "SELECT '';",
    "SELECT ';';",
    "SELECT 'abc';",
    "SELECT ''';';",
    NULL
  };
  size_t i;
  for (i = 0; statements[i]; ++i) {
    COMPARE_STRING(result.at(i), statements[i]);
  }
  COMPARE_NUMBER(result.size(), i);
}

static test_register t("pg_split_statement", test);

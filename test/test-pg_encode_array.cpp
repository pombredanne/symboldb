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

#include <cxxll/pg_encode_array.hpp>

#include "test.hpp"

using namespace cxxll;

static void
test()
{
  std::vector<std::string> v;
  COMPARE_STRING(pg_encode_array(v), "{}");
  v.push_back("");
  COMPARE_STRING(pg_encode_array(v), "{\"\"}");
  v.push_back("abc \" ' \\ xyz");
  COMPARE_STRING(pg_encode_array(v), "{\"\",\"abc \\\" ' \\\\ xyz\"}");
  v.push_back(",");
  COMPARE_STRING(pg_encode_array(v), "{\"\",\"abc \\\" ' \\\\ xyz\",\",\"}");
}

static test_register t("pg_encode_array", test);

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

#include <cxxll/base16.hpp>
#include "test.hpp"

using namespace cxxll;

static void
test()
{
  std::string str;
  std::string result("X");
  base16_decode(str.begin(), str.end(), result.begin());
  COMPARE_STRING(result, "X");
  str = "40414243";
  result = "XXXXY";
  base16_decode(str.begin(), str.end(), result.begin());
  str = "2f3FfFFe";
  base16_decode(str.begin(), str.end(), result.begin());
  COMPARE_STRING(result, "/?\377\376Y");
  result = "XXXXY";
  base16_decode(str.c_str(), result.begin());
  COMPARE_STRING(result, "/?\377\376Y");
}

static test_register t("base16", test);

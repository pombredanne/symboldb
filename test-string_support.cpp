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

#include "string_support.hpp"
#include "test.hpp"

static void
test()
{
  COMPARE_STRING(quote(""), "");
  COMPARE_STRING(quote("a"), "a");
  COMPARE_STRING(quote("a\t"), "a\\t");
  COMPARE_STRING(quote("a\r"), "a\\r");
  COMPARE_STRING(quote("a\n"), "a\\n");
  COMPARE_STRING(quote("a\tb"), "a\\tb");
  COMPARE_STRING(quote("a b"), "a b");
  COMPARE_STRING(quote("a\377b"), "a\\xffb");
  COMPARE_STRING(quote("a\177b"), "a\\x7fb");
  COMPARE_STRING(quote("a\200b"), "a\\x80b");
  COMPARE_STRING(quote(std::string("a\000b\377c", 6)), "a\\x00b\\xffc\\x00");
}

static test_register t("string_support", test);

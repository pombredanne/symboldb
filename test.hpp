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

#pragma once

// Records an error if EXPR evaluates to false.
#define CHECK(expr) test_check((expr), #expr, __FILE__, __LINE__);
void test_check(bool expr, const char *str, const char *file, unsigned line);


// Registers a test case in the test suite.  This is supposed to be
// used to define a file-level static object per test case.
struct test_register {
  test_register(const char *name, void(*)());
  ~test_register();
};

int run_tests();

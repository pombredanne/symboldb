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

#include <string>
#include <sstream>

// Records an error if EXPR evaluates to false.
#define CHECK(expr) test_check((expr), #expr, __FILE__, __LINE__);
void test_check(bool expr, const char *str, const char *file, unsigned line);

#define COMPARE_STRING(left, right) \
  test_compare_string((left), (right), #left, #right, __FILE__, __LINE__)
// Multiple definitions to deal with NULL arguments.
void test_compare_string(const std::string &left, const std::string &right,
			 const char *left_str, const char *right_str,
			 const char *file, unsigned line);
void test_compare_string(const char *left, const char *right,
			 const char *left_str, const char *right_str,
			 const char *file, unsigned line);
void test_compare_string(const char *left, const std::string &right,
			 const char *left_str, const char *right_str,
			 const char *file, unsigned line);
void test_compare_string(const std::string &left, const char *right,
			 const char *left_str, const char *right_str,
			 const char *file, unsigned line);

#define COMPARE_NUMBER(left, right) \
  test_compare_number((left), (right), #left, #right, __FILE__, __LINE__)

void test_compare_number_fail
  (const std::string &left, const std::string &right,
   const char *left_str, const char *right_str,
   const char *file, unsigned line);
void test_compare_number_success();

template <class Left, class Right> void
test_compare_number(Left left, Right right,
		    const char *left_str, const char *right_str,
		    const char *file, unsigned line)
{
  if (left != right) {
    std::ostringstream leftstream;
    leftstream << left;
    std::ostringstream rightstream;
    rightstream << right;
    test_compare_number_fail(leftstream.str(), rightstream.str(),
			     left_str, right_str,
			     file, line);
  } else {
    test_compare_number_success();
  }
}

// Registers a test case in the test suite.  This is supposed to be
// used to define a file-level static object per test case.
struct test_register {
  test_register(const char *name, void(*)());
  ~test_register();
};

int run_tests();

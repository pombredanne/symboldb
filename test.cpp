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

#include "test.hpp"

#include <cstdio>
#include <typeinfo>
#include <vector>

struct test_case {
  const char *name;
  void (*func)();

  test_case(const char *n, void(*f)())
    : name(n), func(f)
  {
  }
};

typedef std::vector<test_case> test_suite;
static test_suite *tests;

test_register::test_register(const char *name, void(*func)())
{
  if (tests == 0) {
    tests = new test_suite;
  }
  tests->push_back(test_case(name, func));
}

test_register::~test_register()
{
}

static const char *current_test;
static bool first_failure;
static unsigned exception_count;
static unsigned success_count;
static unsigned failure_count;

static void
test_header()
{
  if (first_failure) {
    fprintf(stderr, "error: in test case %s:\n", current_test);
    first_failure = false;
  }
}

void
test_check(bool expr, const char *str, const char *file, unsigned line)
{
  if (expr) {
    ++success_count;
    return;
  }
  ++failure_count;
  test_header();
  fprintf(stderr, "%s:%u: failed check: %s\n", file, line, str);
}

void
test_compare_string(const std::string &left, const std::string &right,
		    const char *left_str, const char *right_str,
		    const char *file, unsigned line)
{
  if (left == right) {
    ++success_count;
    return;
  }
  ++failure_count;
  test_header();
  fprintf(stderr, "%s:%u: string comparison failure\n", file, line);
  fprintf(stderr, "%s:%u:   left: %s\n", file, line, left.c_str());
  fprintf(stderr, "%s:%u:     evaluated from: %s\n", file, line, left_str);
  fprintf(stderr, "%s:%u:   right: %s\n", file, line, right.c_str());
  fprintf(stderr, "%s:%u:     evaluated from: %s\n", file, line, right_str);
}

int
run_tests()
{
  for (test_suite::iterator p = tests->begin(), end = tests->end();
       p != end; ++p) {
    current_test = p->name;
    first_failure = true;
    try {
      p->func();
    } catch (std::exception &e) {
      test_header();
      fprintf(stderr, "error: %s: exception %s: %s\n",
	      p->name, typeid(e).name(), e.what());
    } catch (...) {
      test_header();
      fprintf(stderr, "error: %s: unknown exception\n", p->name);
    }
  }
  if (exception_count > 0 || failure_count > 0) {
    fprintf(stderr, "error: %u tests of %u failed, %u exceptions\n",
	    failure_count, failure_count + success_count, exception_count);
    return 1;
  }
  fprintf(stderr, "info: %u tests successful\n",
	  success_count + failure_count);
  return 0;
}

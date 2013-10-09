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
#include <cxxll/dir_handle.hpp>
#include <cxxll/os.hpp>
#include <cxxll/os_exception.hpp>
#include <cxxll/string_support.hpp>
#include <cxxll/pg_exception.hpp>
#include <cxxll/fd_handle.hpp>
#include <cxxll/elf_image.hpp>
#include <cxxll/rpm_parser.hpp>
#include <cxxll/curl_fetch_result.hpp>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <set>
#include <typeinfo>
#include <vector>

using namespace cxxll;

namespace {
  struct test_case {
    const char *name;
    void (*func)();

    test_case(const char *n, void(*f)())
      : name(n), func(f)
    {
    }
  };

  bool
  operator<(const test_case &left, const test_case &right)
  {
    return strcmp(left.name, right.name) < 0;
  }

  typedef std::set<test_case> test_suite;
  static test_suite *tests;

  static struct tests_cleanup {
    ~tests_cleanup()
    {
      delete tests;
    }
  } tests_cleanup_;

  const char *current_test;
  unsigned exception_count;
  unsigned success_count;
  unsigned failure_count;
}

//////////////////////////////////////////////////////////////////////
// test_section

static std::string test_section_name;

test_section::test_section(const char *name)
{
  old_name = name;
  std::swap(test_section_name, old_name);
}

test_section::test_section(const std::string &name)
{
  old_name = name;
  std::swap(test_section_name, old_name);
}

test_section::~test_section()
{
  std::swap(test_section_name, old_name);
}

//////////////////////////////////////////////////////////////////////
// test_register

test_register::test_register(const char *name, void(*func)())
{
  if (tests == 0) {
    tests = new test_suite;
  }
  if (!tests->insert(test_case(name, func)).second) {
    ++failure_count;
    fprintf(stderr, "error: duplicate test case name: %s\n", name);
  }
}

test_register::~test_register()
{
}

//////////////////////////////////////////////////////////////////////

static void
test_header()
{
  static const char *last_test;
  static std::string last_section;
  if (current_test != last_test || last_section != test_section_name) {
    if (test_section_name.empty()) {
      fprintf(stderr, "error: in test case %s:\n", current_test);
    } else {
      fprintf(stderr, "error: in test case %s, \"%s\":\n", current_test,
	      quote(test_section_name).c_str());
    }
    last_test = current_test;
    last_section = test_section_name;
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

static void
test_compare_string(const std::string &left, bool left_null,
		    const std::string &right, bool right_null,
		    const char *left_str, const char *right_str,
		    const char *file, unsigned line)
{
  if (left == right && !left_null && !right_null) {
    ++success_count;
    return;
  }
  ++failure_count;
  test_header();
  fprintf(stderr, "%s:%u: string comparison failure\n", file, line);
  if (left_null) {
    fprintf(stderr, "%s:%u:   left:  (null)\n", file, line);
  } else {
    fprintf(stderr, "%s:%u:   left:  \"%s\"\n",
	    file, line, quote(left).c_str());
  }
  fprintf(stderr, "%s:%u:     evaluated from: %s\n", file, line, left_str);
  if (right_null) {
    fprintf(stderr, "%s:%u:  right:  (null)\n", file, line);
  } else {
    fprintf(stderr, "%s:%u:   right: \"%s\"\n",
	    file, line, quote(right).c_str());
  }
  fprintf(stderr, "%s:%u:     evaluated from: %s\n", file, line, right_str);
}

void test_compare_string(const std::string &left, const std::string &right,
			 const char *left_str, const char *right_str,
			 const char *file, unsigned line)
{
  test_compare_string(left, false,
		      right, false,
		      left_str, right_str, file, line);
}

void test_compare_string(const char *left, const char *right,
			 const char *left_str, const char *right_str,
			 const char *file, unsigned line)
{
  test_compare_string(left == NULL ? "" : left, left == NULL,
		      right == NULL ? "" : right, right == NULL,
		      left_str, right_str, file, line);
}

void test_compare_string(const char *left, const std::string &right,
			 const char *left_str, const char *right_str,
			 const char *file, unsigned line)
{
  test_compare_string(left == NULL ? "" : left, left == NULL,
		      right, false,
		      left_str, right_str, file, line);
}

void test_compare_string(const std::string &left, const char *right,
			 const char *left_str, const char *right_str,
			 const char *file, unsigned line)
{
  test_compare_string(left, false,
		      right == NULL ? "" : right, right == NULL,
		      left_str, right_str, file, line);
}

void
test_compare_number_fail(const std::string &left, const std::string &right,
			 const char *left_str, const char *right_str,
			 const char *file, unsigned line)
{
  ++failure_count;
  test_header();
  fprintf(stderr, "%s:%u: numeric comparison failure\n", file, line);
  fprintf(stderr, "%s:%u:   left:  %s\n", file, line, left.c_str());
  fprintf(stderr, "%s:%u:     evaluated from: %s\n", file, line, left_str);
  fprintf(stderr, "%s:%u:   right: %s\n", file, line, right.c_str());
  fprintf(stderr, "%s:%u:     evaluated from: %s\n", file, line, right_str);
}

void test_compare_number_success()
{
  ++success_count;
}

static std::vector<int>
file_descriptors()
{
  dir_handle fds("/proc/self/fd");
  int self = fds.dirfd();
  std::vector<int> result;
  while (dirent *e = fds.readdir()) {
    unsigned long long fdu;
    if (!parse_unsigned_long_long(e->d_name, fdu)
	|| static_cast<unsigned long long>(static_cast<int>(fdu)) != fdu) {
      fprintf(stderr, "warning: invalid /proc/self/fd entry: %s\n", e->d_name);
    } else {
      int fd = fdu;
      if (fd != self) {
	result.push_back(fd);
      }
    }
  }
  std::sort(result.begin(), result.end());
  return result;
}


static bool
file_descriptors_valid(const std::vector<int> &fd)
{
  return fd.size() == 3
    && fd.at(0) == 0 && fd.at(1) == 1 && fd.at(2) == 2;
}

static void
report_descriptor(const char *prefix, int fd)
{
  char buf[128];
  snprintf(buf, sizeof(buf), "/proc/self/fd/%d", fd);
  std::string path;
  try {
    path = readlink(buf);
  } catch (os_exception &) {
    // ignore errors
  }
  if (path.empty()) {
    fprintf(stderr, "%sdescriptor %d: unknown\n", prefix, fd);
  } else {
    fprintf(stderr, "%sdescriptor %d: \"%s\"\n",
	    prefix, fd, quote(path).c_str());
  }
}

static void
report_descriptors(const char *prefix, const std::vector<int> &fds, bool all)
{
  for (std::vector<int>::const_iterator p = fds.begin(), end = fds.end();
       p != end; ++p) {
    int fd = *p;
    if (fd < 0 || fd > 2 || all) {
      report_descriptor(prefix, fd);
    }
  }
}


int
run_tests()
{
  std::vector<int> start_fds(file_descriptors());
  if (!file_descriptors_valid(start_fds)) {
    fprintf(stderr, "warning: invalid set of file descriptors:\n");
    report_descriptors("warning:   ", start_fds, true);
  }

  {
    // Reserve low file descriptors so that those are available for
    // test cases.  Otherwise, some crypto libraries may pick file
    // descriptors there.
    fd_handle reservation[5];
    for (unsigned i = 0; i < 5; ++i) {
      reservation[i].open_read_only("/dev/null");
    }
    elf_image_init();
    rpm_parser_init();
    curl_fetch_result::global_init();
  }

  for (test_suite::iterator p = tests->begin(), end = tests->end();
       p != end; ++p) {
    current_test = p->name;
    try {
      p->func();
    } catch (pg_exception &e) {
      ++exception_count;
      fprintf(stderr, "error: from PostgreSQL:\n");
      dump("error: ", e, stderr);
    } catch (std::exception &e) {
      ++exception_count;
      test_header();
      fprintf(stderr, "error: %s: exception %s: %s\n",
	      p->name, typeid(e).name(), e.what());
    } catch (...) {
      ++exception_count;
      test_header();
      fprintf(stderr, "error: %s: unknown exception\n", p->name);
    }
  }
  if (exception_count > 0 || failure_count > 0) {
    fprintf(stderr, "error: %u tests of %u failed, %u exceptions\n",
	    failure_count, failure_count + success_count, exception_count);
    return 1;
  }

  rpm_parser_deinit();
  curl_fetch_result::global_deinit();

  if (file_descriptors_valid(start_fds)) {
    std::vector<int> end_fds(file_descriptors());
    if (!file_descriptors_valid(end_fds)) {
      fprintf(stderr, "error: leaked file descriptors:\n");
      report_descriptors("error:   ", end_fds, false);
      return 1;
    }
  }

  fprintf(stderr, "info: %u tests successful\n",
	  success_count + failure_count);
  return 0;
}

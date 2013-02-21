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

#include "fd_handle.hpp"
#include "os_exception.hpp"
#include "test.hpp"

#include <sstream>
#include <unistd.h>

static void
test(void)
{
  COMPARE_STRING(os_exception(0).what(), "os_exception");
  CHECK(os_exception(1).error_code() == 1);
  COMPARE_STRING(os_exception(0).fd(0).what(), "fd=0");
  COMPARE_STRING(os_exception(0).message("abc").what(), "message=abc");
  COMPARE_STRING(os_exception(0).message("abc").message(), "abc");
  COMPARE_STRING(os_exception(0).path("abc").what(), "path=abc");
  COMPARE_STRING(os_exception(0).path("abc").path(), "abc");
  COMPARE_STRING(os_exception(0).path2("abc").what(), "path2=abc");
  COMPARE_STRING(os_exception(0).path2("abc").path2(), "abc");
  COMPARE_STRING(os_exception(0).path("abc").path2("xyz").what(),
		 "path=abc path2=xyz");
  COMPARE_STRING(os_exception(0).offset(0).what(), "offset=0");
  COMPARE_STRING(os_exception(0).length(0).what(), "length=0");
  COMPARE_STRING(os_exception(0).count(0).what(), "count=0");
  {
    os_exception e(0);
    e.function(::getpid);
    CHECK(e.error_code() == 0);
    CHECK(e.function() == ::getpid);
    std::string f(e.function_name());
    CHECK(f.find('[') != std::string::npos);
    f.resize(f.find('['));
    COMPARE_STRING(f, "getpid");
  }

  {
    fd_handle h;
    h.open_read_only("/dev/null");
    std::ostringstream s;
    s << "fd=" << h.raw << " path=/dev/null";
    COMPARE_STRING(os_exception(0).fd(h.raw).defaults().what(),
		   s.str() + " offset=0 length=0");
    os_exception e(0);
    e.fd(h.raw).offset(1).length(2).count(3).defaults();
    COMPARE_STRING(e.what(), s.str() + " offset=1 length=2 count=3");
    CHECK(e.offset() == 1U);
    CHECK(e.length() == 2U);
    CHECK(e.count() == 3U);
  }
}

static test_register t("os_exception", test);

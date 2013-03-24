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

#include <cxxll/java_class.hpp>
#include <cxxll/read_file.hpp>

#include <algorithm>

#include "test.hpp"

using namespace cxxll;

static void
test()
{
  std::vector<unsigned char> vec;
  read_file("test/data/JavaClass.class", vec);
  {
    java_class jc(&vec);
    COMPARE_STRING(jc.this_class(), "com/redhat/symboldb/test/JavaClass");
    COMPARE_STRING(jc.super_class(), "java/lang/Thread");
    CHECK(jc.interface_count() == 2);
    COMPARE_STRING(jc.interface(0), "java/lang/Runnable");
    COMPARE_STRING(jc.interface(1), "java/lang/AutoCloseable");
    {
      std::vector<std::string> classes(jc.class_references());
      std::sort(classes.begin(), classes.end());
      const char *expected[] = {
	"com/redhat/symboldb/test/JavaClass",
	"java/lang/AutoCloseable",
	"java/lang/Byte",
	"java/lang/Double",
	"java/lang/Exception",
	"java/lang/Float",
	"java/lang/Integer",
	"java/lang/Long",
	"java/lang/Runnable",
	"java/lang/Short",
	"java/lang/StackOverflowError",
	"java/lang/StringBuilder",
	"java/lang/Thread",
	NULL
      };
      for (unsigned i = 0; i <= classes.size(); ++i) {
	if (i == classes.size()) {
	  CHECK(expected[i] == NULL);
	} else if (expected[i] == NULL) {
	  CHECK(false);
	  break;
	} else {
	  COMPARE_STRING(classes.at(i), expected[i]);
	}
      }
    }
  }
}

static test_register t("java_class", test);

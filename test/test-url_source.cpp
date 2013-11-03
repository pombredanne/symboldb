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

#include <cxxll/url_source.hpp>
#include <cxxll/string_sink.hpp>
#include <cxxll/source_sink.hpp>
#include <cxxll/read_file.hpp>

#include "test.hpp"

using namespace cxxll;

static void
test()
{
  url_source src("file:///etc/passwd");
  string_sink sink;
  copy_source_to_sink(src, sink);
  std::vector<unsigned char> expected;
  read_file("/etc/passwd", expected);
  COMPARE_STRING(sink.data, std::string(expected.begin(), expected.end()));
  COMPARE_NUMBER(static_cast<size_t>(expected.size()), src.file_size());
}

static test_register t("url_source", test);

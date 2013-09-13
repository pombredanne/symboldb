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

#include <cxxll/read_lines.hpp>
#include <cxxll/file_handle.hpp>
#include <cxxll/malloc_handle.hpp>
#include <cxxll/os_exception.hpp>

#include <errno.h>

using namespace cxxll;

void
cxxll::read_lines(const char *path, std::vector<std::string> &result)
{
  file_handle fh(path, "r");
  file_handle::line line;
  while (fh.getline(line)) {
    if (!line.strip_nl()) {
      throw os_exception(EINVAL).function_name("cxxll::read_sorted_file")
	.path(path).message("missing line feed at end of file");
    }
    result.push_back(line.str());
  }
}

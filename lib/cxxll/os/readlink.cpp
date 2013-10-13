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

#include <cxxll/os.hpp>
#include <cxxll/os_exception.hpp>
#include <cxxll/raise.hpp>

#include <vector>

#include <assert.h>
#include <unistd.h>

using namespace cxxll;

std::string
cxxll::readlink(const char *path)
{
  char buf[4096];
  ssize_t ret = ::readlink(path, buf, sizeof(buf));
  if (ret < 0) {
    throw os_exception().function(::readlink).path(path);
  } if (ret == sizeof(buf)) {
    std::vector<char> vec(2 * sizeof(buf));
    while (true) {
      ret = ::readlink(path, vec.data(), vec.size());
      if (ret < 0) {
	throw os_exception().function(readlink).path(path);
      } else if (static_cast<size_t>(ret) != vec.size()) {
	break;
      }

      size_t new_size = 2 * vec.size();
      if (new_size < vec.size()) {
	raise<std::bad_alloc>();
      }
      vec.resize(new_size);
    }
    return std::string(vec.begin(), vec.end());
  }
  return std::string(buf, buf + ret);
}


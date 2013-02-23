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

#include "os.hpp"
#include "os_exception.hpp"

#include <vector>

#include <assert.h>
#include <errno.h>
#include <unistd.h>

std::string
current_directory()
{
  char buf[4096];
  char *ret = getcwd(buf, sizeof(buf));
  if (ret == NULL) {
    if (errno != ERANGE) {
      throw os_exception().function(getcwd);
    }
    std::vector<char> vec(2 * sizeof(buf));
    while (true) {
      ret = getcwd(vec.data(), vec.size());
      if (ret != NULL) {
	break;
      }
      if (errno != ERANGE) {
	throw os_exception().function(getcwd);
      }

      size_t new_size = 2 * vec.size();
      if (new_size < vec.size()) {
	throw std::bad_alloc();
      }
      vec.resize(new_size);
    }
    return std::string(vec.data());
  }
  return std::string(buf);
}


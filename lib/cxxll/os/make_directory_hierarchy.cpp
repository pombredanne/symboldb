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

#include <assert.h>
#include <errno.h>
#include <sys/stat.h>
#include <string.h>

#include <string>

using namespace cxxll;
bool
cxxll::make_directory_hierarchy(const char *path, unsigned mode)
{
  int ret = mkdir(path, mode);
  if (ret == 0) {
    return true;
  }
  if (errno == EEXIST) {
    return is_directory(path);
  }

  std::string s;
  const char *p = path;
  while (*p == '/') {
    ++p;
    s += '/';
  }
  while (*p) {
    assert(*p != '/');
    const char *slash = strchr(p, '/');
    if (!slash) {
      slash = p + strlen(p);
    }
    s.append(p, slash);

    // Ignore intermediate errors, only the final result counts.
    mkdir(s.c_str(), mode);

    p = slash;
    while (*p == '/') {
      ++p;
      s += '/';
    }
  }

  return is_directory(path);
}

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

#include <stdlib.h>
#include <string.h>

#include <vector>

using namespace cxxll;

std::string
cxxll::make_temporary_directory(const char *prefix)
{
  std::vector<char> templ(prefix, prefix + strlen(prefix));
  static const char placeholder[7] = "XXXXXX";
  templ.insert(templ.end(), placeholder, placeholder + sizeof(placeholder));
  const char *ret = mkdtemp(templ.data());
  if (ret == nullptr) {
    throw os_exception().path(prefix).function(mkdtemp);
  }
  return ret;
}

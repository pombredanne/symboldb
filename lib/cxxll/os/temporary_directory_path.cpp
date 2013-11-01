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

#include <stdlib.h>
#include <unistd.h>

using namespace cxxll;

std::string
cxxll::temporary_directory_path()
{
  const char *tmp = getenv("TMPDIR");
  if (tmp != nullptr && is_directory(tmp)
      && access(tmp, R_OK | W_OK | X_OK) == 0) {
    return tmp;
  }
  return "/tmp";
}

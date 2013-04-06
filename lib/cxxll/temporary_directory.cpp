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

#include <cxxll/temporary_directory.hpp>
#include <cxxll/os.hpp>

using namespace cxxll;

temporary_directory::temporary_directory()
  : path_(make_temporary_directory
	  ((temporary_directory_path() + "/tmp-").c_str()))
{
}

temporary_directory::temporary_directory(const char *prefix)
  : path_(make_temporary_directory(prefix))
{
}

temporary_directory::~temporary_directory()
{
  if (!path_.empty()) {
    try {
      remove_directory_tree(path_.c_str());
    } catch (...) {
      // TODO: log
    }
  }
}

void
temporary_directory::remove()
{
  try {
    remove_directory_tree(path_.c_str());
  } catch (...) {
    path_.clear();
    throw;
  }
  path_.clear();
}

std::string
temporary_directory::path(const char *name) const
{
  std::string result(path_);
  result += '/';
  result += name;
  return result;
}

/*
 * Copyright (C) 2012, 2013 Red Hat, Inc.
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

#include <cxxll/rpm_file_info.hpp>
#include <cxxll/utf8.hpp>

#include <stdlib.h>
#include <sys/stat.h>

using namespace cxxll;

rpm_file_info::rpm_file_info()
  : mode(0), mtime(0), ino(0), nlinks(0), normalized(false)
{
}

rpm_file_info::~rpm_file_info()
{
}

bool
rpm_file_info::is_directory() const
{
  return S_ISDIR(mode);
}

bool
rpm_file_info::is_symlink() const
{
  return S_ISLNK(mode);
}

void
rpm_file_info::normalize_name()
{
  if (!normalized && !is_valid_utf8(name)) {
    name = latin1_to_utf8(name);
    normalized = true;
  }
}

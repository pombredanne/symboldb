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

#pragma once

#include <stdint.h>
#include <sys/types.h>

#include <string>

// Information about a file in an RPM.
// This data comes from the RPM header, not the cpio section.
struct rpm_file_info {
  std::string name;
  std::string user;
  std::string group;
  uint32_t mode;
  uint32_t mtime;
  bool normalized; // See normalize_name() below.

  rpm_file_info();
  ~rpm_file_info();

  // Some of the names are not encoded as UTF-8.  We pamper over that
  // by re-encoding from ISO-8859-1 to UTF-8.  Sets normalized to
  // true.
  void normalize_name();
};

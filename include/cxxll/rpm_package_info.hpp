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

#include <string>

namespace cxxll {

// Information about an entire RPM package.
struct rpm_package_info {
  std::string name;
  std::string version;
  std::string release;
  std::string arch;
  std::string source_rpm;
  std::string hash;		// 40 hexadecimal characters
  std::string build_host;
  std::string summary;
  std::string description;
  std::string license;
  std::string group;
  long long build_time{};
  int epoch{-1};		// -1 if no epoch
  enum {binary, source} kind{binary};
  bool normalized{};

  rpm_package_info();
  ~rpm_package_info();

  // Force invalid encodings in header fields to UTF-8.
  // Currently, this covers the license member only.
  void normalize();
};

} // namespace cxxll

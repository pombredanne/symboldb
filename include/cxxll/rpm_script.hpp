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

#pragma once

#include <string>
#include <vector>

namespace cxxll {

struct rpm_script {
  typedef enum {
    pretrans,
    prein,
    postin,
    preun,
    postun,
    posttrans,
    verify
  } kind;

  // Converts the script kind to the string.
  static const char *to_string(kind);

  explicit rpm_script(kind);
  ~rpm_script();

  std::string script;
  std::vector<std::string> prog;
  kind type;
  bool script_present;
};

} // namespace cxxll

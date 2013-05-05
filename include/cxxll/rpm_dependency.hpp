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

namespace cxxll {

struct rpm_dependency {
  typedef enum {
    undefined,
    requires,
    provides,
    conflicts,
    obsoletes
  } kind_type;

  std::string capability;
  std::string op;
  std::string version;

  kind_type kind;
  bool pre;
  bool build;

  rpm_dependency();
  ~rpm_dependency();
};

}

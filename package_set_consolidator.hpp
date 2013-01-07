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

#include <tr1/memory>
#include <vector>

#include "database.hpp"

class rpm_package_info;

// Picks the largest version number for each package name/architecture
// combinations.
class package_set_consolidator {
  struct impl;
  std::tr1::shared_ptr<impl> impl_;
public:
  package_set_consolidator();
  ~package_set_consolidator();

  void add(const rpm_package_info &, database::package_id);
  std::vector<database::package_id> package_ids() const;
};

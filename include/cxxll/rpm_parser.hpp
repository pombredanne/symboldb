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

#include "rpm_dependency.hpp"

#include <tr1/memory>
#include <vector>

namespace cxxll {

class rpm_file_entry;
class rpm_package_info;
class rpm_script;
class rpm_trigger;

// This needs to be called once before creating any rpm_parser_state
// objects.
void rpm_parser_init();

// Attempts to clean up the RPM library.
void rpm_parser_deinit();

class rpm_parser_state {
  struct impl;
  std::tr1::shared_ptr<impl> impl_;
public:
  // Opens the RPM file at PATH.
  explicit rpm_parser_state(const char *path);
  ~rpm_parser_state();

  const char *nevra() const;
  const rpm_package_info &package() const;

  const std::vector<rpm_dependency> &dependencies() const;

  // Adds the non-trigger scripts from the RPM header to the vector.
  void scripts(std::vector<rpm_script> &) const;

  // Replaces the trigger scripts from the RPM header to the vector.
  void triggers(std::vector<rpm_trigger> &) const;

  // Reads the next payload entry.  Returns true if an entry has been
  // read, false on EOF.  Throws rpm_parser_exception on read errors.
  bool read_file(rpm_file_entry &);
};

} // namespace cxxll

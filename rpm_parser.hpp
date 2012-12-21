/*
 * Copyright (C) 2012 Red Hat, Inc.
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

#include "rpm_file_info.hpp"

#include <stdexcept>
#include <tr1/memory>
#include <vector>

class rpm_package_info;

// This needs to be called once before creating any rpm_parser_state
// objects.
void rpm_parser_init();

struct rpm_file_entry {
  std::tr1::shared_ptr<rpm_file_info> info;
  std::vector<char> contents;

  rpm_file_entry();
  ~rpm_file_entry();
};

class rpm_parser_state {
  struct impl;
  std::tr1::shared_ptr<impl> impl_;
public:
  // Opens the RPM file at PATH.
  rpm_parser_state(const char *path);
  ~rpm_parser_state();

  const char *nevra() const;
  const rpm_package_info &package() const;

  // Reads the next payload entry.  Returns true if an entry has been
  // read, false on EOF.  Throws rpm_parser_exception on read errors.
  bool read_file(rpm_file_entry &);
};

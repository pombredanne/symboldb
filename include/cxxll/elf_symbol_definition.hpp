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

#include "elf_symbol.hpp"

namespace cxxll {

struct elf_symbol_definition : elf_symbol {
  std::string vda_name;
  unsigned long long value;
  unsigned short section;
  unsigned xsection;
  bool default_version;

  // Returns true if xsection is actually present.
  bool has_xsection() const;

  elf_symbol_definition();
  ~elf_symbol_definition();
};

} // namespace cxxll

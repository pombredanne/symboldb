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

#include <cxxll/elf_symbol_definition.hpp>

#include <elf.h>

using namespace cxxll;

elf_symbol_definition::elf_symbol_definition()
  : value(0ULL), section(0), xsection(0), default_version(false)
{
}

elf_symbol_definition::~elf_symbol_definition()
{
}

bool
elf_symbol_definition::has_xsection() const
{
  return section == SHN_XINDEX;
}

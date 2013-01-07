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

#include "elf_symbol.hpp"

#include <gelf.h>
#include <assert.h>

elf_symbol::elf_symbol()
  : other(0)
{
}

elf_symbol::~elf_symbol()
{
}

const char *
elf_symbol::visibility() const
{
  switch (GELF_ST_VISIBILITY(other)) {
  case STV_DEFAULT:
    return "default";
  case STV_INTERNAL:
    return "internal";
  case STV_HIDDEN:
    return "hidden";
  case STV_PROTECTED:
    return "protected";
  default:
    assert(0 && "invalid visibility value");
  }
}

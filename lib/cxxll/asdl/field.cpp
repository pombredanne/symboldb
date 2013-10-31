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

#include <cxxll/asdl.hpp>

cxxll::asdl::field::field()
  : cardinality(single)
{
}

cxxll::asdl::field::~field()
{
}

std::ostream &
cxxll::asdl::operator<<(std::ostream &os, const field &fld)
{
  os << fld.type << fld.cardinality;
  if (!fld.name.empty()) {
    os << ' ' << fld.name;
  }
  return os;
}

std::ostream &
cxxll::asdl::operator<<(std::ostream &os, const std::vector<field> &fld)
{
  bool first = true;
  for (std::vector<field>::const_iterator p = fld.begin(), end = fld.end();
       p != end; ++p) {
    if (first) {
      first = false;
    } else {
      os << ',';
    }
    os << *p;
  }
  return os;
}

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

#include <cxxll/pg_encode_array.hpp>
#include <cxxll/pg_exception.hpp>

std::string
cxxll::pg_encode_array(const std::vector<std::string> &in)
{
  std::string result("{");
  for (std::vector<std::string>::const_iterator
	 p = in.begin(), end = in.end(); p != end; ++p) {
    if (result.size() > 1) {
      result += ',';
    }
    result += '"';
    for (std::string::const_iterator
	   q = p->begin(), qend = p->end(); q != qend; ++q) {
      char ch = *q;
      switch (ch) {
      case 0:
	throw pg_exception("NUL character in pg_encode_array");
      case '"':
      case '\\':
	result += '\\';
	// fallthrough
      default:
	result += ch;
      }
    }
    result += '"';
  }
  result += '}';
  return result;
}

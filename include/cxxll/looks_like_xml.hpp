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

namespace cxxll {

template <class InputIterator>
bool
looks_like_xml(InputIterator first, InputIterator last)
{
  while (first != last) {
    unsigned char ch = *first;
    ++first;
    if (ch == '<') {
      // Start of an XML tag, processing directive etc.
      return true;
    }
    switch (ch) {
    case 0x20:
    case 0x09:
    case 0x0D:
    case 0x0A:
      // Permitted white space according to the specification.
      continue;
    case 0xEF:
      // Potential byte order mark.
      if (first != last) {
	ch = *first;
	++first;
	if (ch == 0xBB && first != last) {
	  ch = *first;
	  ++first;
	  if (ch == 0xBF) {
	    continue;
	  }
	}
      }
      return false;
    default:
      return false;
    }
  }
  return false;
}


}

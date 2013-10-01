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

inline const char *
char_cast(const char *s)
{
  return s;
}

inline const char *
char_cast(const unsigned char *s)
{
  return reinterpret_cast<const char *>(s);
}

inline const char *
char_cast(const signed char *s)
{
  return reinterpret_cast<const char *>(s);
}

inline char *
char_cast(char *s)
{
  return s;
}

inline char *
char_cast(unsigned char *s)
{
  return reinterpret_cast<char *>(s);
}

inline char *
char_cast(signed char *s)
{
  return reinterpret_cast<char *>(s);
}


} // namespace cxll

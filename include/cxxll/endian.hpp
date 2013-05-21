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

#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__ && __BYTE_ORDER__ != __ORDER_BIG_ENDIAN__
#error unknown endianess
#endif

namespace cxxll {

inline bool
is_little_endian()
{
  return __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__;
}

inline bool
is_big_endian()
{
  return __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__;
}

inline unsigned
cpu_to_le_32(unsigned val)
{
  if (is_little_endian()) {
    return val;
  } else {
    return __builtin_bswap32(val);
  }
}

inline unsigned long long
cpu_to_le_64(unsigned long long val)
{
  if (is_little_endian()) {
    return val;
  } else {
    return __builtin_bswap64(val);
  }
}

inline unsigned
cpu_to_be_32(unsigned val)
{
  if (is_big_endian()) {
    return val;
  } else {
    return __builtin_bswap32(val);
  }
}

inline unsigned long long
cpu_to_be_64(unsigned long long val)
{
  if (is_big_endian()) {
    return val;
  } else {
    return __builtin_bswap64(val);
  }
}

inline unsigned
le_to_cpu_32(unsigned val)
{
  return cpu_to_le_32(val);
}

inline unsigned long long
le_to_cpu_64(unsigned long long val)
{
  return cpu_to_le_64(val);
}

inline unsigned
be_to_cpu_32(unsigned val)
{
  return cpu_to_be_32(val);
}

inline unsigned long long
be_to_cpu_64(unsigned long long val)
{
  return cpu_to_be_64(val);
}

} // namespace cxxll

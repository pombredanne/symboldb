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

#include <cxxll/endian.hpp>
#include "test.hpp"

static void
test()
{
  union {
    char buf[8];
    unsigned u32;
    unsigned long u64;
  } values;

  values.u32 = cpu_to_le_32(0x01020304);
  COMPARE_STRING(std::string(values.buf, values.buf + 4), "\x04\x03\x02\x01");
  values.u32 = cpu_to_be_32(0x01020304);
  COMPARE_STRING(std::string(values.buf, values.buf + 4), "\x01\x02\x03\x04");
  values.u32 = cpu_to_le_32(0xf1f2f3f4);
  COMPARE_STRING(std::string(values.buf, values.buf + 4), "\xf4\xf3\xf2\xf1");
  values.u32 = cpu_to_be_32(0xf1f2f3f4);
  COMPARE_STRING(std::string(values.buf, values.buf + 4), "\xf1\xf2\xf3\xf4");

  values.u64 = cpu_to_le_64(0x0102030405060708);
  COMPARE_STRING(std::string(values.buf, values.buf + 8),
		 "\x08\x07\x06\x05\x04\x03\x02\x01");
  values.u64 = cpu_to_be_64(0x0102030405060708);
  COMPARE_STRING(std::string(values.buf, values.buf + 8),
		 "\x01\x02\x03\x04\x05\x06\x07\x08");
  values.u64 = cpu_to_le_64(0xf1f2f3f4f5f6f7f8);
  COMPARE_STRING(std::string(values.buf, values.buf + 8),
		 "\xf8\xf7\xf6\xf5\xf4\xf3\xf2\xf1");
  values.u64 = cpu_to_be_64(0xf1f2f3f4f5f6f7f8);
  COMPARE_STRING(std::string(values.buf, values.buf + 8),
		 "\xf1\xf2\xf3\xf4\xf5\xf6\xf7\xf8");
}

static test_register t("endian", test);

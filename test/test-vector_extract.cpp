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

#include <cxxll/vector_extract.hpp>

#include "test.hpp"

using namespace cxxll;

static void
test()
{
  {
    std::vector<unsigned char> vec;
    std::string target("abc");
    extract(vec, 0, 0, target);
    CHECK(target.empty());
    target = "A SAMPLE STRING";
    vec.insert(vec.end(), target.begin(), target.end());
    target.clear();
    extract(vec, 0, vec.size(), target);
    COMPARE_STRING(target, "A SAMPLE STRING");
    extract(vec, 1, vec.size() - 2, target);
    COMPARE_STRING(target, " SAMPLE STRIN");
    CHECK(extract8(vec, 0) == 'A');
    CHECK(extract8(vec, 1) == ' ');
    size_t offset = 0;
    unsigned char ch = 0;
    extract(vec, offset, ch);
    CHECK(offset = 1);
    CHECK(ch == 'A');
    extract(vec, offset, ch);
    CHECK(offset = 2);
    CHECK(ch == ' ');
  }
  {
    using namespace cxxll::big_endian;
    const std::string ref("\001\002\003\004\005\006\007\010"
			  "\370\371\372\373\374\375\376\377");
    std::vector<unsigned char> vec;
    vec.insert(vec.end(), ref.begin(), ref.end());
    CHECK(vec.size() == 16);
    {
      CHECK(extract16(vec, 0) == 0x0102);
      CHECK(extract16(vec, 8) == 0xF8F9);
      CHECK(extract16(vec, 14) == 0xFEFF);
      size_t offset = 0;
      unsigned short val;
      extract(vec, offset, val);
      CHECK(val == 0x0102);
      CHECK(offset == 2);
      extract(vec, offset, val);
      CHECK(val == 0x0304);
      CHECK(offset == 4);
    }
    {
      CHECK(extract32(vec, 0) == 0x01020304);
      CHECK(extract32(vec, 8) == 0xF8F9FAFBU);
      CHECK(extract32(vec, 12) == 0xFCFDFEFFU);
      size_t offset = 0;
      unsigned val;
      extract(vec, offset, val);
      CHECK(val == 0x01020304);
      CHECK(offset == 4);
      extract(vec, offset, val);
      CHECK(val == 0x05060708);
      CHECK(offset == 8);
    }
    {
      CHECK(extract64(vec, 0) == 0x0102030405060708ULL);
      CHECK(extract64(vec, 8) == 0xF8F9FAFBFCFDFEFFULL);
      size_t offset = 0;
      unsigned long long val;
      extract(vec, offset, val);
      CHECK(val == 0x0102030405060708ULL);
      CHECK(offset == 8);
      extract(vec, offset, val);
      CHECK(val == 0xF8F9FAFBFCFDFEFFULL);
      CHECK(offset == 16);
    }
  }
}

static test_register t("vector_extract", test);

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

#include <cxxll/const_stringref.hpp>
#include <cxxll/bad_string_index.hpp>

#include <sstream>

#include "test.hpp"

using namespace cxxll;

static const char *string7 = "string7";
static const char *string8 = "string8";

static void
test()
{
  try {
    const_stringref s;
    CHECK(s.data());
    CHECK(s.empty());
    COMPARE_NUMBER(s.size(), 0);
    COMPARE_NUMBER(s.nlen(), 0);
    s.at(0);
    CHECK(false);
  } catch (bad_string_index &e) {
    COMPARE_NUMBER(e.index(), 0);
    COMPARE_NUMBER(e.size(), 0);
    COMPARE_STRING(e.what(), "size = 0, index = 0");
  }

  // Constructor overloads.
  COMPARE_STRING
    (const_stringref(reinterpret_cast<const unsigned char *>("string")).str(),
     "string");
  COMPARE_STRING(const_stringref(string7, 6).str(), "string");
  COMPARE_STRING
    (const_stringref(reinterpret_cast<const unsigned char *>(string7), 6)
     .str(), "string");
  COMPARE_STRING
    (const_stringref(static_cast<const void *>(string7), 6).str(), "string");
  COMPARE_STRING
    (const_stringref(std::vector<char>(string7, string7 + 6)).str(), "string");
  COMPARE_STRING
    (const_stringref(std::vector<unsigned char>(string7, string7 + 6)).str(),
     "string");

  // data() pointer preservation
  CHECK(const_stringref(string7).data() == string7);
  CHECK((const_stringref(string7) + 1).data() == string7 + 1);
  CHECK((const_stringref(string7) + 7).data() == string7 + 7);
  { const_stringref s(string7); ++s; CHECK(s.data() == string7 + 1); }
  { const_stringref s(string7); s++; CHECK(s.data() == string7 + 1); }
  { const_stringref s(string7); s += 1; CHECK(s.data() == string7 + 1); }
  
  // empty()
  CHECK(!const_stringref(string7).empty());
  CHECK(const_stringref(string7).substr(7).empty());

  // Lexicographical ordering.
  CHECK(const_stringref(string7) < const_stringref(string7, 8));
  CHECK(!(const_stringref(string7) > const_stringref(string7, 8)));
  CHECK(const_stringref(string7) <= const_stringref(string7, 8));
  CHECK(!(const_stringref(string7) >= const_stringref(string7, 8)));
  CHECK(!(const_stringref(string7) == const_stringref(string7, 8)));
  CHECK(const_stringref(string7) <= const_stringref(string7));
  CHECK(const_stringref(string7) >= const_stringref(string7));
  CHECK(!(const_stringref(string7) < const_stringref(string7)));
  CHECK(!(const_stringref(string7) > const_stringref(string7)));
  CHECK(const_stringref(string7) != const_stringref(string7, 8));
  CHECK(!(const_stringref(string7) != const_stringref(string7)));
  CHECK(const_stringref(string7) < string8);
  CHECK(const_stringref(string7) <= string8);
  CHECK(!(const_stringref(string7) > string8));
  CHECK(!(const_stringref(string7) >= string8));
  CHECK(!(const_stringref(string7) == const_stringref(string8)));
  CHECK(const_stringref(string7) != const_stringref(string8));
  
  try {
    const_stringref s;
    s[0];
    CHECK(false);
  } catch (bad_string_index &e) {
    COMPARE_NUMBER(e.index(), 0);
    COMPARE_NUMBER(e.size(), 0);
    COMPARE_STRING(e.what(), "size = 0, index = 0");
  }

  {
    char buf[2] = "a";
    const_stringref s(buf);
    COMPARE_NUMBER(s.size(), 1);
    COMPARE_NUMBER(s.nlen(), 1);
    COMPARE_NUMBER(s.at(0), 'a');
    COMPARE_NUMBER(s[0], 'a');
    COMPARE_STRING(s.str(), "a");
    buf[0] = 'b';
    COMPARE_NUMBER(s.at(0), 'b');
    COMPARE_NUMBER(s[0], 'b');
    COMPARE_STRING(s.str(), "b");

    COMPARE_NUMBER((s + 1).size(), 0);
    COMPARE_STRING((s + 1).str(), "");

    try {
      s.at(1);
      CHECK(false);
    } catch (bad_string_index &e) {
      COMPARE_NUMBER(e.index(), 1);
      COMPARE_NUMBER(e.size(), 1);
      COMPARE_STRING(e.what(), "size = 1, index = 1");
    }
    try {
      s[1];
      CHECK(false);
    } catch (bad_string_index &e) {
      COMPARE_NUMBER(e.index(), 1);
      COMPARE_NUMBER(e.size(), 1);
      COMPARE_STRING(e.what(), "size = 1, index = 1");
    }
    try {
      s + size_t(-1);
      CHECK(false);
    } catch (bad_string_index &e) {
      COMPARE_NUMBER(e.index(), -1);
      COMPARE_NUMBER(e.size(), 1);
    }

    const_stringref s1;
    s1 = s;
    COMPARE_NUMBER((++s1).size(), 0);
    s1 = s;
    COMPARE_NUMBER((s1++).size(), 1);
    s1 = s;
    CHECK((s1++) == "b");
    s1 = s;
    COMPARE_STRING((s1++).str(), "b");
    s1 = s;
    s1 += 0;
    COMPARE_STRING(s1.str(), "b");
    COMPARE_NUMBER(s1.size(), 1);
    s1 += 1;
    COMPARE_NUMBER(s1.size(), 0);
    COMPARE_STRING(s1.str(), "");
    CHECK(const_stringref(s1) == (s1 += 0));

    try {
      ++s1;
      CHECK(false);
    } catch (bad_string_index &e) {
      COMPARE_NUMBER(e.index(), 0);
      COMPARE_NUMBER(e.size(), 0);
      COMPARE_STRING(e.what(), "size = 0, index = 0");
    }
    try {
      s1++;
      CHECK(false);
    } catch (bad_string_index &e) {
      COMPARE_NUMBER(e.index(), 0);
      COMPARE_NUMBER(e.size(), 0);
      COMPARE_STRING(e.what(), "size = 0, index = 0");
    }

    buf[0] = '\0';
    COMPARE_NUMBER(s.size(), 1);
    COMPARE_NUMBER(s.nlen(), 0);
  }

  {
    const_stringref s("LONG STRING");
    COMPARE_STRING(s.substr(0).str(), "LONG STRING");
    COMPARE_STRING(s.substr(0, 4).str(), "LONG");
    COMPARE_STRING(s.substr(1).str(), "ONG STRING");
    COMPARE_STRING(s.substr(s.size() - 1).str(), "G");
    COMPARE_STRING(s.substr(s.size()).str(), "");

    try {
      s.substr(-1);
      CHECK(false);
    } catch (bad_string_index &e) {
      COMPARE_NUMBER(e.index(), -1);
      COMPARE_NUMBER(e.size(), s.size());
    }
    try {
      s.substr(s.size() + 1);
      CHECK(false);
    } catch (bad_string_index &e) {
      COMPARE_NUMBER(e.index(), s.size() + 1);
      COMPARE_NUMBER(e.size(), s.size());
    }
    try {
      s.substr(s.size() + 1, 0);
      CHECK(false);
    } catch (bad_string_index &e) {
      COMPARE_NUMBER(e.index(), s.size() + 1);
      COMPARE_NUMBER(e.size(), s.size());
    }
  }

  {
    std::ostringstream os;
    os << const_stringref("abc") << const_stringref("\000XYZ", 5);
    COMPARE_STRING(os.str(), std::string("abc\000XYZ", 8));
  }

  {
    std::string str;
    str += "";
    str += const_stringref("abc");
    str += const_stringref("\000XYZ", 5);
    COMPARE_STRING(str, std::string("abc\000XYZ", 8));
  }
}

static test_register t("const_stringref", test);

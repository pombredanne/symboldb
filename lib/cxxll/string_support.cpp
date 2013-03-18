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

#include <cxxll/string_support.hpp>

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <sstream>

using namespace cxxll;

std::string
cxxll::quote(const std::string &str)
{
  for (const char *p = str.data(), *end = p + str.size(); p != end; ++p) {
    unsigned char ch = *p;
    if (ch <= ' ' || ch == '\\' || ch == '"' || ch == '\'' || ch >= 0x7f) {
      std::ostringstream s;
      s << std::hex;
      // Restart from the beginning.
      for (p = str.data(); p != end; ++p) {
	unsigned char ch = *p;
	switch (ch) {
	case '\r':
	  s << "\\r";
	  break;
	case '\n':
	  s << "\\n";
	  break;
	case '\t':
	  s << "\\t";
	  break;
	case '"':
	case '\'':
	case '\\':
	  s << '\\' << ch;
	  break;
	default:
	  if (ch < ' ' || ch >= 0x7f) {
	    s << "\\x";
	    s.width(2);
	    s.fill('0');
	    s << (int)ch;
	  } else {
	    s << ch;
	  }
	}
      }
      return s.str();
    }
  }
  // No special characters found.
  return str;
}

namespace {
  const char *
  non_whitespace(const char *first, const char *last)
  {
    for (; first != last; ++first) {
      if (whitespace(*first)) {
	continue;
      }
      break;
    }
    return first;
  }

  void
  strip_inplace(std::string &s)
  {
    std::string::iterator p(s.begin());
    std::string::iterator end(s.end());
    while (p != end && whitespace(*p)) {
      ++p;
    }
    s.erase(s.begin(), p);
    p = s.end();
    end = s.begin();
    while (p != end) {
      --p;
      if (!whitespace(*p)) {
	++p;
	s.erase(p, s.end());
	break;
      }
    }
  }
}

// Parses an unsigned long long while skipping white space.
bool
cxxll::parse_unsigned_long_long(const std::string &text,
				unsigned long long &value)
{
  const char *first = text.c_str();
  const char *last = first + text.size();
  first = non_whitespace(first, last);
  errno = 0;
  char *endptr;
  unsigned long long v = strtoull(first, &endptr, 10);
  if (errno != 0) {
    return false;
  }
  if (non_whitespace(endptr, last) != last) {
    // Trailing non-whitespace.
    return false;
  }
  value = v;
  return true;
}

std::string
cxxll::strip(const std::string &s)
{
  std::string r(s);
  strip_inplace(r);
  return r;
}

unsigned
cxxll::fnv(const unsigned char *p, size_t len)
{
  unsigned hash = 0x243f6a88;
  while (len) {
    hash ^= *p;
    hash *= 0x9e3779b9;
    --len;
    ++p;
  }
  return hash;
}

unsigned
cxxll::fnv(const std::string &str)
{
  return fnv(reinterpret_cast<const unsigned char *>(str.data()), str.size());
}

unsigned
cxxll::fnv(const char *str)
{
  return fnv(reinterpret_cast<const unsigned char *>(str), strlen(str));
}

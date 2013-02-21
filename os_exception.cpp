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

#include "os_exception.hpp"
#include "os.hpp"

#include <sstream>

#include <errno.h>

os_exception::os_exception()
{
  init();
  error_code_ = errno;
}

os_exception::os_exception(int code)
{
  init();
  error_code_ = code;
}

void
os_exception::init()
{
  function_ = NULL;
  offset_ = 0;
  length_ = 0;
  count_ = 0;
  fd_ = -1;
  error_code_ = errno;
  bad_alloc_ = false;
  offset_set_ = false;
  length_set_ = false;
  count_set_ = false;
}

os_exception::~os_exception() throw()
{
}

os_exception &
os_exception::set(std::string &target, const char *str)
{
  try {
    target = str;
  } catch (std::bad_alloc &) {
    bad_alloc_ = true;
  }
  return *this;
}

os_exception &
os_exception::set(std::string &target, const std::string &str)
{
  try {
    target = str;
  } catch (std::bad_alloc &) {
    bad_alloc_ = true;
  }
  return *this;
}

os_exception &
os_exception::message(const char *str)
{
  return set(message_, str);
}

os_exception &
os_exception::message(const std::string &str)
{
  return set(message_, str);
}

os_exception &
os_exception::path(const char *str)
{
  return set(path_, str);
}

os_exception &
os_exception::path(const std::string &str)
{
  return set(path_, str);
}

os_exception &
os_exception::path2(const char *str)
{
  return set(path2_, str);
}

os_exception &
os_exception::path2(const std::string &str)
{
  return set(path2_, str);
}

namespace {
  void
  maybe_quote(std::ostream &s, const std::string str)
  {
    for (const char *p = str.data(), *end = p + str.size(); p != end; ++p) {
      unsigned char ch = *p;
      if (ch <= ' ' || ch == '\\' || ch == '"' || ch == '\'' || ch >= 0x7f) {
	// Restart from the beginning.
	s << '"';
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
	    s << "\\n";
	    break;
	  case '"':
	  case '\'':
	  case '\\':
	    s << '\\' << ch;
	    break;
	  default:
	    if (ch < ' ' || ch >= 0x7f) {
	      s << '\\' << (int)ch; // FIXME
	    } else {
	      s << ch;
	    }
	  }
	}
	s << '"';
	return;
      }
    }
    // No special characters found, output verbatim.
    s << str;
  }

  void
  field(std::ostream &s, const char *name, bool &first)
  {
    if (first) {
      first = false;
    } else {
      s << ' ';
    }
    s << name << '=';
  }

  void
  string_field(std::ostream &s, const char *name, bool &first,
	       const std::string &value)
  {
    if (!value.empty()) {
      field(s, name, first);
      maybe_quote(s, value);
    }
  }

  void
  numeric_field(std::ostream &s, const char *name, bool &first,
		bool present, unsigned long long value)
  {
    if (present) {
      field(s, name, first);
      s << value;
    }
  }
}

const char *
os_exception::what() const throw()
{
  try {
    if (what_.empty()) {
      std::ostringstream s;
      bool first = true;
      string_field(s, "message", first, message_);
      string_field(s, "function", first, function_name_);
      if (error_code_ != 0) {
	field(s, "error", first);
	maybe_quote(s, error_string(error_code_));
      }
      if (fd_ >= 0) {
	field(s, "fd", first);
	s << fd_;
      }
      if (!path_.empty()) {
	field(s, "path", first);
	maybe_quote(s, path_);
      }
      if (!path2_.empty()) {
	field(s, "path2", first);
	maybe_quote(s, path2_);
      }
      numeric_field(s, "offset", first, offset_set_, offset_);
      numeric_field(s, "length", first, length_set_, length_);
      numeric_field(s, "count", first, count_set_, count_);
      if (bad_alloc_) {
	if (first) {
	  s << "no information due to allocation error";
	} else {
	  s << " incomplete";
	}
      }
      if (first) {
	s << "os_exception";
      }
      what_ = s.str();
    }
    return what_.c_str();
  } catch (...) {
    return "out of memory";
  }
}

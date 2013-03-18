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

#include <cxxll/regex_handle.hpp>

#include <regex.h>

struct regex_handle::impl {
  regex_t rx;

  impl(const char *regexp);
  ~impl();

  bool match(const char *subject);
  void throw_error(int code);
};

inline
regex_handle::impl::impl(const char *regexp)
{
  int ret = regcomp(&rx, regexp, REG_EXTENDED | REG_NOSUB);
  if (ret != 0) {
    throw_error(ret);
  }
}

inline
regex_handle::impl::~impl()
{
  regfree(&rx);
}

inline bool
regex_handle::impl::match(const char *subject)
{
  int ret = regexec(&rx, subject, 0, NULL, 0);
  switch (ret) {
  case 0:
    return true;
  case REG_NOMATCH:
    return false;
  default:
    throw_error(ret);
    throw;
  }
}

void
regex_handle::impl::throw_error(int code)
{
  char msg[1024];
  regerror(code, &rx, msg, sizeof(msg));
  throw error(msg, code);
}

regex_handle::regex_handle(const char *regexp)
  : impl_(new impl(regexp))
{
}

regex_handle::~regex_handle()
{
}

bool
regex_handle::match(const char *subject) const
{
  return impl_->match(subject);
}

//////////////////////////////////////////////////////////////////////
// regex_handle::error

regex_handle::error::error(const char *message, int code)
  : what_(message), code_(code)
{
}

regex_handle::error::~error() throw()
{
}

const char *
regex_handle::error::what() const throw()
{
  return what_.c_str();
}

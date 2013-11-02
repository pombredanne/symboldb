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

#include <cxxll/asdl.hpp>

namespace {
  inline bool
  upper(char ch)
  {
    return 'A' <= ch && ch <= 'Z';
  }

  inline bool
  lower(char ch)
  {
    return 'a' <= ch && ch <= 'z';
  }

  inline bool
  digit(char ch)
  {
    return '0' <= ch && ch <= '9';
  }

  bool
  alpha(char ch)
  {
    return upper(ch) || lower(ch);
  }

  bool
  letter(char ch)
  {
    return alpha(ch) || digit(ch) || ch == '_';
  }
}


bool
cxxll::asdl::valid_identifier(const identifier &str)
{
  if (str.empty() || !alpha(str.at(0))) {
    return false;
  }
  std::string::size_type len = str.size();
  for (char ch : str) {
    if (!letter(ch)) {
      return false;
    }
  }
  return true;
}

bool
cxxll::asdl::valid_type(const identifier &str)
{
  return valid_identifier(str) && lower(str.at(0));
}

bool
cxxll::asdl::valid_constructor(const identifier &str)
{
  return valid_identifier(str) && upper(str.at(0));
}

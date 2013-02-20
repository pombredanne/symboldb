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

#include <errno.h>

os_exception::os_exception()
{
  init(errno);
}

os_exception::~os_exception() throw()
{
}

void
os_exception::init(int code)
{
  try {
    message = error_string(code);
  } catch(std::bad_alloc &) {
    // Swallow the exception, see special case in what().
  }
}

const char *
os_exception::what() const throw()
{
  if (message.empty()) {
    return "out of memory";
  } else {
    return message.c_str();
  }
}

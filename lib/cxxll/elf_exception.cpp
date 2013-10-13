/*
 * Copyright (C) 2012, 2013 Red Hat, Inc.
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

#include <cxxll/elf_exception.hpp>
#include <cxxll/raise.hpp>

#include <libelf.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

using namespace cxxll;

elf_exception::elf_exception()
  : std::runtime_error(elf_errmsg(-1))
{
}

elf_exception::elf_exception(const char *msg)
  : std::runtime_error(msg)
{
}

void
elf_exception::raise(const char *msg, ...)
{
  va_list ap;
  char *buffer;
  va_start(ap, msg);
  int ret = vasprintf(&buffer, msg, ap);
  va_end(ap);
  if (ret < 0) {
    cxxll::raise<std::runtime_error>("vasprintf failed");
  }
  try {
    throw elf_exception(buffer);
  } catch (...) {
    free(buffer);
    throw;
  }
}

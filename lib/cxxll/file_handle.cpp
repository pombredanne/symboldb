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

#include <cxxll/file_handle.hpp>
#include <cxxll/os_exception.hpp>

#include <errno.h>

using namespace cxxll;

file_handle::file_handle(FILE *newptr)
  : raw(newptr)
{
  if (raw == NULL) {
    throw os_exception();
  }
}

file_handle::file_handle(const char *path, const char *flags)
  : raw(fopen(path, flags))
{
  if (raw == NULL) {
    throw os_exception().function(fopen).path(path).defaults();
  }
}

file_handle::~file_handle()
{
  if (raw != NULL) {
    fclose(raw);
  }
}

void
file_handle::close()
{
  if (raw != NULL) {
    FILE *ptr = raw;
    raw = NULL;
    if (fclose(ptr) != 0) {
      throw os_exception().function(fclose).defaults();
    }
  }
}

void
file_handle::reset(FILE *newptr)
{
  if (newptr == NULL) {
    throw os_exception();
  }
  try {
    close();
  } catch (...) {
    fclose(newptr);
    throw;
  }
  raw = newptr;
}

bool
file_handle::getline(line &l)
{
  char *lineptr = l.ptr.release();
  errno = 0;
  ssize_t ret = ::getline(&lineptr, &l.allocated, raw);
  int err = errno;
  l.ptr.reset(lineptr);
  if (ret < 0) {
    l.length = 0;
    // Some libc versions return -1 on EOF and clobber errno.
    if (err == 0 || !ferror(raw)) {
      return false;
    } else {
      throw os_exception(err).function(::getline).fd(fileno(raw)).defaults();
    }
  }
  l.length = ret;
  return true;
}

//////////////////////////////////////////////////////////////////////
// cxxll::file_hande::line

cxxll::file_handle::line::line()
  : length(0), allocated(0)
{
}

cxxll::file_handle::line::~line()
{
}

bool
cxxll::file_handle::line::strip_nl()
{
  if (length == 0) {
    return false;
  }
  if (ptr.get()[length - 1] == '\n') {
    --length;
    return true;
  }
  return false;
}

std::string
cxxll::file_handle::line::str() const
{
  return std::string(ptr.get(), length);
}

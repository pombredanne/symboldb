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

#include <cxxll/dir_handle.hpp>
#include <cxxll/os_exception.hpp>

#include <errno.h>
#include <unistd.h>

using namespace cxxll;

dir_handle::dir_handle(int fd)
{
  try {
    raw = fdopendir(fd);
    if (raw == NULL) {
      throw os_exception().function(fdopendir).fd(fd).defaults();
    }
  } catch (...) {
    ::close(fd);
    throw;
  }
}

dir_handle::dir_handle(const char *path)
  : raw(opendir(path))
{
  if (raw == NULL) {
    throw os_exception().function(opendir).path(path);
  }
}

dir_handle::~dir_handle()
{
  if (raw != NULL) {
    closedir(raw);
  }
}

void
dir_handle::close()
{
  if (raw != NULL) {
    int ret = closedir(raw);
    raw = NULL;
    if (ret != 0) {
      throw os_exception().function(closedir);
    }
  }
}

dirent *
dir_handle::readdir()
{
  while (dirent *e = readdir_all()) {
    if (e->d_name[0] == '.'
	&& (e->d_name[1] == '\0'
	    || (e->d_name[1] == '.' && e->d_name[2] == '\0'))) {
      continue;
    }
    return e;
  }
  return NULL;
}

dirent *
dir_handle::readdir_all()
{
  errno = 0;
  dirent *result = ::readdir(raw);
  int err = errno;
  if (result == NULL && err != 0) {
    throw os_exception(err).function(::readdir).fd(dirfd()).defaults();
  }
  return result;
}

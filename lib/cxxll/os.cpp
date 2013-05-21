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

#include <cxxll/malloc_handle.hpp>
#include <cxxll/os.hpp>
#include <cxxll/os_exception.hpp>

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <stdexcept>
#include <vector>

using namespace cxxll;

bool
cxxll::is_directory(const char *path)
{
  struct stat64 st;
  if (stat64(path, &st) != 0) {
    return false;
  }
  return S_ISDIR(st.st_mode);
}

bool
cxxll::is_executable(const char *path)
{
  return access(path, X_OK) == 0;
}

bool
cxxll::path_exists(const char *path)
{
  return access(path, F_OK) == 0;
}

std::string
cxxll::home_directory()
{
  const char *home = getenv("HOME");
  if (home && is_directory(home)) {
    return home;
  }
  // TODO: use getpwuid_r to obtain the directory from the user
  // databse.
  throw std::runtime_error("could not determine HOME directory");
}

std::string
cxxll::temporary_directory_path()
{
  const char *tmp = getenv("TMPDIR");
  if (tmp != NULL && is_directory(tmp)
      && access(tmp, R_OK | W_OK | X_OK) == 0) {
    return tmp;
  }
  return "/tmp";
}

bool
cxxll::make_directory_hierarchy(const char *path, unsigned mode)
{
  int ret = mkdir(path, mode);
  if (ret == 0) {
    return true;
  }
  if (errno == EEXIST) {
    return is_directory(path);
  }

  std::string s;
  const char *p = path;
  while (*p == '/') {
    ++p;
    s += '/';
  }
  while (*p) {
    assert(*p != '/');
    const char *slash = strchr(p, '/');
    if (!slash) {
      slash = p + strlen(p);
    }
    s.append(p, slash);

    // Ignore intermediate errors, only the final result counts.
    mkdir(s.c_str(), mode);

    p = slash;
    while (*p == '/') {
      ++p;
      s += '/';
    }
  }

  return is_directory(path);
}

std::string
cxxll::make_temporary_directory(const char *prefix)
{
  std::vector<char> templ(prefix, prefix + strlen(prefix));
  static const char placeholder[7] = "XXXXXX";
  templ.insert(templ.end(), placeholder, placeholder + sizeof(placeholder));
  const char *ret = mkdtemp(templ.data());
  if (ret == NULL) {
    throw os_exception().path(prefix).function(mkdtemp);
  }
  return ret;
}

std::string
cxxll::realpath(const char *path)
{
  malloc_handle<char> handle(::realpath(path, NULL));
  if (handle.get() == NULL) {
    throw os_exception().function(::realpath).path(path);
  }
  return std::string(handle.get());
}

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

#include "malloc_handle.hpp"
#include "os.hpp"
#include "os_exception.hpp"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <stdexcept>

bool
is_directory(const char *path)
{
  struct stat64 st;
  if (stat64(path, &st) != 0) {
    return false;
  }
  return S_ISDIR(st.st_mode);
}

std::string home_directory()
{
  const char *home = getenv("HOME");
  if (home && is_directory(home)) {
    return home;
  }
  // TODO: use getpwuid_r to obtain the directory from the user
  // databse.
  throw std::runtime_error("could not determine HOME directory");
}

bool
make_directory_hierarchy(const char *path, unsigned mode)
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
realpath(const char *path)
{
  malloc_handle<char> handle;
  handle.raw = realpath(path, NULL);
  if (handle.raw == NULL) {
    throw os_exception().function<char *(const char *, char *)>(realpath)
      .path(path);
  }
  return handle.raw;
}

// We do not know for sure which strerror_r prototype we got.

namespace {
  template <class T>
  struct my_strerror {
  };

  template<>
  struct my_strerror<int (*)(int, char *, size_t)> {
    static std::string op(int (*func)(int, char *, size_t), int code)
    {
      char buf[1024];
      int ret = func(code, buf, sizeof(buf));
      if (ret != 0) {
	snprintf(buf, sizeof(buf), "unknown error %d", code);
      }
      return buf;
    }
  };

  template<>
  struct my_strerror<char *(*)(int, char *, size_t)> {
    static std::string op(char * (*func)(int, char *, size_t), int code)
    {
      char buf[1024];
      char *ptr = func(code, buf, sizeof(buf));
      if (ptr != 0) {
	return ptr;
      }
      snprintf(buf, sizeof(buf), "unknown error %d", code);
      return buf;
    }
  };

  template<class T> inline
  std::string my_strerror_op(T func, int code)
  {
    return my_strerror<T>::op(func, code);
  }
}

std::string
error_string(int code)
{
  return my_strerror_op(strerror_r, code);
}

std::string
error_string()
{
  return error_string(errno);
}

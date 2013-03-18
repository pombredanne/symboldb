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

#pragma once

// This header file contains only internal declarations.  Do not use
// directly.

#include "endian.hpp"

#include <cstring>
#include <string>
#include <vector>

#include <libpq-fe.h>

// Not for direct use.
namespace pg_private {
  template <class T>
  struct dispatch {
  };

  template <>
  struct dispatch<bool> {
    typedef bool arg;
    static const Oid oid = 16;
    static const int storage = 1;
    static const char *store(char *, bool);
    static int length(bool) { return storage; }
    static void load(PGresult *, int row, int col, bool &);
  };

  template <>
  struct dispatch<short> {
    typedef short arg;
    static const Oid oid = 21;
    static const int storage = 2;
    static const char *store(char *, short);
    static int length(short) { return storage; }
    static void load(PGresult *, int row, int col, short &);
  };

  template <>
  struct dispatch<const short> : dispatch<short> {
  };

  template <>
  struct dispatch<int> {
    typedef int arg;
    static const Oid oid = 23;
    static const int storage = 4;
    static const char *store(char *, int);
    static int length(int) { return storage; }
    static void load(PGresult *, int row, int col, int &);
  };

  template <>
  struct dispatch<const int> : dispatch<int> {
  };


  template <>
  struct dispatch<long long> {
    typedef long long arg;
    static const Oid oid = 20;
    static const int storage = 8;
    static const char *store(char *, long long);
    static int length(long long) { return storage; }
    static void load(PGresult *, int row, int col, long long &);
  };

  template <>
  struct dispatch<const long long> : dispatch<long long> {
  };

  template <>
  struct dispatch<const char *> {
    typedef const char *arg;
    static const Oid oid = 25;
    static const int storage = 0;
    static const char *store(char *, const char *);
    static int length(const char *);
  };

  template <>
  struct dispatch<std::string> {
    typedef const std::string &arg;
    static const Oid oid = 25;
    static const int storage = 0;
    static const char *store(char *, const std::string &);
    static int length(const std::string &);
    static void load(PGresult *, int row, int col, std::string &);
  };

  template <>
  struct dispatch<std::vector<unsigned char> > {
    typedef const std::vector<unsigned char> &arg;
    static const Oid oid = 17; // BYTEA
    static const int storage = 0;
    static const char *store(char *, const std::vector<unsigned char> &);
    static int length(const std::vector<unsigned char> &);
    static void load(PGresult *, int row, int col,
		     std::vector<unsigned char> &);
  };

  template <>
  struct dispatch<const std::vector<unsigned char> >
    : dispatch<std::vector<unsigned char> > {
  };

  template <class T>
  struct dispatch<T *> {
    typedef const T *arg;
    static const Oid oid = dispatch<T>::oid;
    static const int storage = dispatch<T>::storage;
    static const char *store(char *, const T *);
    static int length(const T *);
  };

  // Throws std::pg_exception if the length is not representable.
  int length_check(size_t);

  inline const char *
  dispatch<bool>::store(char *buffer, bool val)
  {
    buffer[0] = val;
    return buffer;
  }

  inline const char *
  dispatch<short>::store(char *buffer, short val)
  {
    buffer[0] = val >> 8;
    buffer[1] = val;
    return buffer;
  }

  inline const char *
  dispatch<int>::store(char *buffer, int val)
  {
    val = cpu_to_be_32(val);
    __builtin_memcpy(buffer, &val, sizeof(val));
    return buffer;
  }

  inline const char *
  dispatch<long long>::store(char *buffer, long long val)
  {
    val = cpu_to_be_64(val);
    __builtin_memcpy(buffer, &val, sizeof(val));
    return buffer;
  }

  inline int
  dispatch<const char *>::length(const char *str)
  {
    // FIXME: strlen call and NULL check should be out of line in the
    // non-constant case.
    size_t length = str == NULL ? 0 : strlen(str);
    if (__builtin_constant_p(length)) {
      return length;
    } else {
      return length_check(length);
    }
  }

  inline const char *
  dispatch<const char *>::store(char *, const char *str)
  {
    return str;
  }

  inline const char *
  dispatch<std::string>::store(char *, const std::string &str)
  {
    return str.data();
  }

  template <class T> const Oid dispatch<T *>::oid;
  template <class T> const int dispatch<T *>::storage;

  template <class T>
  inline int
  dispatch<T *>::length(const T *ptr)
  {
    if (ptr) {
      return dispatch<T>::length(*ptr);
    }
    return 0;
  }

  template <class T>
  inline const char *
  dispatch<T *>::store(char *buffer, const T *ptr)
  {
    if (ptr) {
      return dispatch<T>::store(buffer, *ptr);
    }
    return 0;
  }
} // pg_private

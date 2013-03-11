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

#include "pgresult_handle.hpp"
#include "endian.hpp"

#include <cstring>
#include <string>

namespace pg_query_private {
  template <class T>
  struct dispatch {
  };

  template <>
  struct dispatch<bool> {
    static const Oid oid = 16;
    static const unsigned storage = 1;
    static const char *store(char *, bool);
    static int length(bool) { return storage; }
  };

  template <>
  struct dispatch<short> {
    static const Oid oid = 21;
    static const unsigned storage = 2;
    static const char *store(char *, short);
    static int length(short) { return storage; }
  };

  template <>
  struct dispatch<const short> : dispatch<short> {
  };

  template <>
  struct dispatch<int> {
    static const Oid oid = 23;
    static const unsigned storage = 4;
    static const char *store(char *, int);
    static int length(int) { return storage; }
  };

  template <>
  struct dispatch<const int> : dispatch<int> {
  };


  template <>
  struct dispatch<long long> {
    static const Oid oid = 20;
    static const unsigned storage = 8;
    static const char *store(char *, long long);
    static int length(long long) { return storage; }
  };

  template <>
  struct dispatch<const long long> : dispatch<long long> {
  };

  template <>
  struct dispatch<const char *> {
    static const Oid oid = 25;
    static const unsigned storage = 0;
    static const char *store(char *, const char *);
    static int length(const char *);
  };

  template <>
  struct dispatch<std::string> {
    static const Oid oid = 25;
    static const unsigned storage = 0;
    static const char *store(char *, const std::string &);
    static int length(const std::string &);
  };

  template <class T>
  struct dispatch<T *> {
    static const Oid oid = dispatch<T>::oid;
    static const unsigned storage = dispatch<T>::storage;
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
    size_t length = strlen(str);
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
  template <class T> const unsigned dispatch<T *>::storage;

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
}

template <class T1> void
pg_query(pgconn_handle &conn, pgresult_handle &res, const char *command,
	 T1 t1)
{
  char s1[pg_query_private::dispatch<T1>::storage];
  const Oid paramTypes[] = {
    pg_query_private::dispatch<T1>::oid,
  };
  const char * paramValues[] = {
    pg_query_private::dispatch<T1>::store(s1, t1),
  };
  const int paramLengths[] = {
    pg_query_private::dispatch<T1>::length(t1),
  };
  const int paramFormats[] = {
    1,
  };
  res.execTypedParams
    (conn, command, paramTypes, paramValues, paramLengths, paramFormats);
}

template <class T1, class T2> void
pg_query(pgconn_handle &conn, pgresult_handle &res, const char *command,
	 T1 t1, T2 t2)
{
  char s1[pg_query_private::dispatch<T1>::storage];
  char s2[pg_query_private::dispatch<T2>::storage];
  const Oid paramTypes[] = {
    pg_query_private::dispatch<T1>::oid,
    pg_query_private::dispatch<T2>::oid,
  };
  const char * paramValues[] = {
    pg_query_private::dispatch<T1>::store(s1, t1),
    pg_query_private::dispatch<T2>::store(s2, t2),
  };
  const int paramLengths[] = {
    pg_query_private::dispatch<T1>::length(t1),
    pg_query_private::dispatch<T2>::length(t2),
  };
  const int paramFormats[] = {
    1,
    1,
  };
  res.execTypedParams
    (conn, command, paramTypes, paramValues, paramLengths, paramFormats);
}

template <class T1, class T2, class T3> void
pg_query(pgconn_handle &conn, pgresult_handle &res, const char *command,
	 T1 t1, T2 t2, T3 t3)
{
  char s1[pg_query_private::dispatch<T1>::storage];
  char s2[pg_query_private::dispatch<T2>::storage];
  char s3[pg_query_private::dispatch<T3>::storage];
  const Oid paramTypes[] = {
    pg_query_private::dispatch<T1>::oid,
    pg_query_private::dispatch<T2>::oid,
    pg_query_private::dispatch<T3>::oid,
  };
  const char * paramValues[] = {
    pg_query_private::dispatch<T1>::store(s1, t1),
    pg_query_private::dispatch<T2>::store(s2, t2),
    pg_query_private::dispatch<T3>::store(s3, t3),
  };
  const int paramLengths[] = {
    pg_query_private::dispatch<T1>::length(t1),
    pg_query_private::dispatch<T2>::length(t2),
    pg_query_private::dispatch<T3>::length(t3),
  };
  const int paramFormats[] = {
    1,
    1,
    1,
  };
  res.execTypedParams
    (conn, command, paramTypes, paramValues, paramLengths, paramFormats);
}

template <class T1, class T2, class T3, class T4> void
pg_query(pgconn_handle &conn, pgresult_handle &res, const char *command,
	 T1 t1, T2 t2, T3 t3, T4 t4)
{
  char s1[pg_query_private::dispatch<T1>::storage];
  char s2[pg_query_private::dispatch<T2>::storage];
  char s3[pg_query_private::dispatch<T3>::storage];
  char s4[pg_query_private::dispatch<T4>::storage];
  const Oid paramTypes[] = {
    pg_query_private::dispatch<T1>::oid,
    pg_query_private::dispatch<T2>::oid,
    pg_query_private::dispatch<T3>::oid,
    pg_query_private::dispatch<T4>::oid,
  };
  const char * paramValues[] = {
    pg_query_private::dispatch<T1>::store(s1, t1),
    pg_query_private::dispatch<T2>::store(s2, t2),
    pg_query_private::dispatch<T3>::store(s3, t3),
    pg_query_private::dispatch<T4>::store(s4, t4),
  };
  const int paramLengths[] = {
    pg_query_private::dispatch<T1>::length(t1),
    pg_query_private::dispatch<T2>::length(t2),
    pg_query_private::dispatch<T3>::length(t3),
    pg_query_private::dispatch<T4>::length(t4),
  };
  const int paramFormats[] = {
    1,
    1,
    1,
    1,
  };
  res.execTypedParams
    (conn, command, paramTypes, paramValues, paramLengths, paramFormats);
}

template <class T1, class T2, class T3, class T4, class T5> void
pg_query(pgconn_handle &conn, pgresult_handle &res, const char *command,
	 T1 t1, T2 t2, T3 t3, T4 t4, T5 t5)
{
  char s1[pg_query_private::dispatch<T1>::storage];
  char s2[pg_query_private::dispatch<T2>::storage];
  char s3[pg_query_private::dispatch<T3>::storage];
  char s4[pg_query_private::dispatch<T4>::storage];
  char s5[pg_query_private::dispatch<T5>::storage];
  const Oid paramTypes[] = {
    pg_query_private::dispatch<T1>::oid,
    pg_query_private::dispatch<T2>::oid,
    pg_query_private::dispatch<T3>::oid,
    pg_query_private::dispatch<T4>::oid,
    pg_query_private::dispatch<T5>::oid,
  };
  const char * paramValues[] = {
    pg_query_private::dispatch<T1>::store(s1, t1),
    pg_query_private::dispatch<T2>::store(s2, t2),
    pg_query_private::dispatch<T3>::store(s3, t3),
    pg_query_private::dispatch<T4>::store(s4, t4),
    pg_query_private::dispatch<T5>::store(s5, t5),
  };
  const int paramLengths[] = {
    pg_query_private::dispatch<T1>::length(t1),
    pg_query_private::dispatch<T2>::length(t2),
    pg_query_private::dispatch<T3>::length(t3),
    pg_query_private::dispatch<T4>::length(t4),
    pg_query_private::dispatch<T5>::length(t5),
  };
  const int paramFormats[] = {
    1,
    1,
    1,
    1,
    1,
  };
  res.execTypedParams
    (conn, command, paramTypes, paramValues, paramLengths, paramFormats);
}

template <class T1, class T2, class T3, class T4, class T5, class T6> void
pg_query(pgconn_handle &conn, pgresult_handle &res, const char *command,
	 T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6)
{
  char s1[pg_query_private::dispatch<T1>::storage];
  char s2[pg_query_private::dispatch<T2>::storage];
  char s3[pg_query_private::dispatch<T3>::storage];
  char s4[pg_query_private::dispatch<T4>::storage];
  char s5[pg_query_private::dispatch<T5>::storage];
  char s6[pg_query_private::dispatch<T6>::storage];
  const Oid paramTypes[] = {
    pg_query_private::dispatch<T1>::oid,
    pg_query_private::dispatch<T2>::oid,
    pg_query_private::dispatch<T3>::oid,
    pg_query_private::dispatch<T4>::oid,
    pg_query_private::dispatch<T5>::oid,
    pg_query_private::dispatch<T6>::oid,
  };
  const char * paramValues[] = {
    pg_query_private::dispatch<T1>::store(s1, t1),
    pg_query_private::dispatch<T2>::store(s2, t2),
    pg_query_private::dispatch<T3>::store(s3, t3),
    pg_query_private::dispatch<T4>::store(s4, t4),
    pg_query_private::dispatch<T5>::store(s5, t5),
    pg_query_private::dispatch<T6>::store(s6, t6),
  };
  const int paramLengths[] = {
    pg_query_private::dispatch<T1>::length(t1),
    pg_query_private::dispatch<T2>::length(t2),
    pg_query_private::dispatch<T3>::length(t3),
    pg_query_private::dispatch<T4>::length(t4),
    pg_query_private::dispatch<T5>::length(t5),
    pg_query_private::dispatch<T6>::length(t6),
  };
  const int paramFormats[] = {
    1,
    1,
    1,
    1,
    1,
    1,
  };
  res.execTypedParams
    (conn, command, paramTypes, paramValues, paramLengths, paramFormats);
}

template <class T1, class T2, class T3, class T4, class T5, class T6,
	  class T7> void
pg_query(pgconn_handle &conn, pgresult_handle &res, const char *command,
	 T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7)
{
  char s1[pg_query_private::dispatch<T1>::storage];
  char s2[pg_query_private::dispatch<T2>::storage];
  char s3[pg_query_private::dispatch<T3>::storage];
  char s4[pg_query_private::dispatch<T4>::storage];
  char s5[pg_query_private::dispatch<T5>::storage];
  char s6[pg_query_private::dispatch<T6>::storage];
  char s7[pg_query_private::dispatch<T7>::storage];
  const Oid paramTypes[] = {
    pg_query_private::dispatch<T1>::oid,
    pg_query_private::dispatch<T2>::oid,
    pg_query_private::dispatch<T3>::oid,
    pg_query_private::dispatch<T4>::oid,
    pg_query_private::dispatch<T5>::oid,
    pg_query_private::dispatch<T6>::oid,
    pg_query_private::dispatch<T7>::oid,
  };
  const char * paramValues[] = {
    pg_query_private::dispatch<T1>::store(s1, t1),
    pg_query_private::dispatch<T2>::store(s2, t2),
    pg_query_private::dispatch<T3>::store(s3, t3),
    pg_query_private::dispatch<T4>::store(s4, t4),
    pg_query_private::dispatch<T5>::store(s5, t5),
    pg_query_private::dispatch<T6>::store(s6, t6),
    pg_query_private::dispatch<T7>::store(s7, t7),
  };
  const int paramLengths[] = {
    pg_query_private::dispatch<T1>::length(t1),
    pg_query_private::dispatch<T2>::length(t2),
    pg_query_private::dispatch<T3>::length(t3),
    pg_query_private::dispatch<T4>::length(t4),
    pg_query_private::dispatch<T5>::length(t5),
    pg_query_private::dispatch<T6>::length(t6),
    pg_query_private::dispatch<T7>::length(t7),
  };
  const int paramFormats[] = {
    1,
    1,
    1,
    1,
    1,
    1,
    1,
  };
  res.execTypedParams
    (conn, command, paramTypes, paramValues, paramLengths, paramFormats);
}

template <class T1, class T2, class T3, class T4, class T5, class T6,
	  class T7, class T8> void
pg_query(pgconn_handle &conn, pgresult_handle &res, const char *command,
	 T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7, T8 t8)
{
  char s1[pg_query_private::dispatch<T1>::storage];
  char s2[pg_query_private::dispatch<T2>::storage];
  char s3[pg_query_private::dispatch<T3>::storage];
  char s4[pg_query_private::dispatch<T4>::storage];
  char s5[pg_query_private::dispatch<T5>::storage];
  char s6[pg_query_private::dispatch<T6>::storage];
  char s7[pg_query_private::dispatch<T7>::storage];
  char s8[pg_query_private::dispatch<T8>::storage];
  const Oid paramTypes[] = {
    pg_query_private::dispatch<T1>::oid,
    pg_query_private::dispatch<T2>::oid,
    pg_query_private::dispatch<T3>::oid,
    pg_query_private::dispatch<T4>::oid,
    pg_query_private::dispatch<T5>::oid,
    pg_query_private::dispatch<T6>::oid,
    pg_query_private::dispatch<T7>::oid,
    pg_query_private::dispatch<T8>::oid,
  };
  const char * paramValues[] = {
    pg_query_private::dispatch<T1>::store(s1, t1),
    pg_query_private::dispatch<T2>::store(s2, t2),
    pg_query_private::dispatch<T3>::store(s3, t3),
    pg_query_private::dispatch<T4>::store(s4, t4),
    pg_query_private::dispatch<T5>::store(s5, t5),
    pg_query_private::dispatch<T6>::store(s6, t6),
    pg_query_private::dispatch<T7>::store(s7, t7),
    pg_query_private::dispatch<T8>::store(s8, t8),
  };
  const int paramLengths[] = {
    pg_query_private::dispatch<T1>::length(t1),
    pg_query_private::dispatch<T2>::length(t2),
    pg_query_private::dispatch<T3>::length(t3),
    pg_query_private::dispatch<T4>::length(t4),
    pg_query_private::dispatch<T5>::length(t5),
    pg_query_private::dispatch<T6>::length(t6),
    pg_query_private::dispatch<T7>::length(t7),
    pg_query_private::dispatch<T8>::length(t8),
  };
  const int paramFormats[] = {
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
  };
  res.execTypedParams
    (conn, command, paramTypes, paramValues, paramLengths, paramFormats);
}

template <class T1, class T2, class T3, class T4, class T5, class T6,
	  class T7, class T8, class T9> void
pg_query(pgconn_handle &conn, pgresult_handle &res, const char *command,
	 T1 t1, T2 t2, T3 t3, T4 t4, T5 t5, T6 t6, T7 t7, T8 t8, T9 t9)
{
  char s1[pg_query_private::dispatch<T1>::storage];
  char s2[pg_query_private::dispatch<T2>::storage];
  char s3[pg_query_private::dispatch<T3>::storage];
  char s4[pg_query_private::dispatch<T4>::storage];
  char s5[pg_query_private::dispatch<T5>::storage];
  char s6[pg_query_private::dispatch<T6>::storage];
  char s7[pg_query_private::dispatch<T7>::storage];
  char s8[pg_query_private::dispatch<T8>::storage];
  char s9[pg_query_private::dispatch<T9>::storage];
  const Oid paramTypes[] = {
    pg_query_private::dispatch<T1>::oid,
    pg_query_private::dispatch<T2>::oid,
    pg_query_private::dispatch<T3>::oid,
    pg_query_private::dispatch<T4>::oid,
    pg_query_private::dispatch<T5>::oid,
    pg_query_private::dispatch<T6>::oid,
    pg_query_private::dispatch<T7>::oid,
    pg_query_private::dispatch<T8>::oid,
    pg_query_private::dispatch<T9>::oid,
  };
  const char * paramValues[] = {
    pg_query_private::dispatch<T1>::store(s1, t1),
    pg_query_private::dispatch<T2>::store(s2, t2),
    pg_query_private::dispatch<T3>::store(s3, t3),
    pg_query_private::dispatch<T4>::store(s4, t4),
    pg_query_private::dispatch<T5>::store(s5, t5),
    pg_query_private::dispatch<T6>::store(s6, t6),
    pg_query_private::dispatch<T7>::store(s7, t7),
    pg_query_private::dispatch<T8>::store(s8, t8),
    pg_query_private::dispatch<T9>::store(s9, t9),
  };
  const int paramLengths[] = {
    pg_query_private::dispatch<T1>::length(t1),
    pg_query_private::dispatch<T2>::length(t2),
    pg_query_private::dispatch<T3>::length(t3),
    pg_query_private::dispatch<T4>::length(t4),
    pg_query_private::dispatch<T5>::length(t5),
    pg_query_private::dispatch<T6>::length(t6),
    pg_query_private::dispatch<T7>::length(t8),
    pg_query_private::dispatch<T8>::length(t8),
    pg_query_private::dispatch<T9>::length(t9),
  };
  const int paramFormats[] = {
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
  };
  res.execTypedParams
    (conn, command, paramTypes, paramValues, paramLengths, paramFormats);
}

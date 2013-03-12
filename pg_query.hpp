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

#include "pg_private.hpp"
#include "pgresult_handle.hpp"

template <class T1> inline void
pg_query(pgconn_handle &conn, pgresult_handle &res, const char *command,
	 const T1 &t1)
{
  char s1[pg_private::dispatch<T1>::storage];
  const Oid paramTypes[] = {
    pg_private::dispatch<T1>::oid,
  };
  const char * paramValues[] = {
    pg_private::dispatch<T1>::store(s1, t1),
  };
  const int paramLengths[] = {
    pg_private::dispatch<T1>::length(t1),
  };
  const int paramFormats[] = {
    1,
  };
  res.execTypedParams
    (conn, command, paramTypes, paramValues, paramLengths, paramFormats);
}

template <class T1, class T2> inline void
pg_query(pgconn_handle &conn, pgresult_handle &res, const char *command,
	 const T1 &t1, const T2 &t2)
{
  char s1[pg_private::dispatch<T1>::storage];
  char s2[pg_private::dispatch<T2>::storage];
  const Oid paramTypes[] = {
    pg_private::dispatch<T1>::oid,
    pg_private::dispatch<T2>::oid,
  };
  const char * paramValues[] = {
    pg_private::dispatch<T1>::store(s1, t1),
    pg_private::dispatch<T2>::store(s2, t2),
  };
  const int paramLengths[] = {
    pg_private::dispatch<T1>::length(t1),
    pg_private::dispatch<T2>::length(t2),
  };
  const int paramFormats[] = {
    1,
    1,
  };
  res.execTypedParams
    (conn, command, paramTypes, paramValues, paramLengths, paramFormats);
}

template <class T1, class T2, class T3> inline void
pg_query(pgconn_handle &conn, pgresult_handle &res, const char *command,
	 const T1 &t1, const T2 &t2, const T3 &t3)
{
  char s1[pg_private::dispatch<T1>::storage];
  char s2[pg_private::dispatch<T2>::storage];
  char s3[pg_private::dispatch<T3>::storage];
  const Oid paramTypes[] = {
    pg_private::dispatch<T1>::oid,
    pg_private::dispatch<T2>::oid,
    pg_private::dispatch<T3>::oid,
  };
  const char * paramValues[] = {
    pg_private::dispatch<T1>::store(s1, t1),
    pg_private::dispatch<T2>::store(s2, t2),
    pg_private::dispatch<T3>::store(s3, t3),
  };
  const int paramLengths[] = {
    pg_private::dispatch<T1>::length(t1),
    pg_private::dispatch<T2>::length(t2),
    pg_private::dispatch<T3>::length(t3),
  };
  const int paramFormats[] = {
    1,
    1,
    1,
  };
  res.execTypedParams
    (conn, command, paramTypes, paramValues, paramLengths, paramFormats);
}

template <class T1, class T2, class T3, class T4> inline void
pg_query(pgconn_handle &conn, pgresult_handle &res, const char *command,
	 const T1 &t1, const T2 &t2, const T3 &t3, const T4 &t4)
{
  char s1[pg_private::dispatch<T1>::storage];
  char s2[pg_private::dispatch<T2>::storage];
  char s3[pg_private::dispatch<T3>::storage];
  char s4[pg_private::dispatch<T4>::storage];
  const Oid paramTypes[] = {
    pg_private::dispatch<T1>::oid,
    pg_private::dispatch<T2>::oid,
    pg_private::dispatch<T3>::oid,
    pg_private::dispatch<T4>::oid,
  };
  const char * paramValues[] = {
    pg_private::dispatch<T1>::store(s1, t1),
    pg_private::dispatch<T2>::store(s2, t2),
    pg_private::dispatch<T3>::store(s3, t3),
    pg_private::dispatch<T4>::store(s4, t4),
  };
  const int paramLengths[] = {
    pg_private::dispatch<T1>::length(t1),
    pg_private::dispatch<T2>::length(t2),
    pg_private::dispatch<T3>::length(t3),
    pg_private::dispatch<T4>::length(t4),
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

template <class T1, class T2, class T3, class T4, class T5> inline void
pg_query(pgconn_handle &conn, pgresult_handle &res, const char *command,
	 const T1 &t1, const T2 &t2, const T3 &t3, const T4 &t4, const T5 &t5)
{
  char s1[pg_private::dispatch<T1>::storage];
  char s2[pg_private::dispatch<T2>::storage];
  char s3[pg_private::dispatch<T3>::storage];
  char s4[pg_private::dispatch<T4>::storage];
  char s5[pg_private::dispatch<T5>::storage];
  const Oid paramTypes[] = {
    pg_private::dispatch<T1>::oid,
    pg_private::dispatch<T2>::oid,
    pg_private::dispatch<T3>::oid,
    pg_private::dispatch<T4>::oid,
    pg_private::dispatch<T5>::oid,
  };
  const char * paramValues[] = {
    pg_private::dispatch<T1>::store(s1, t1),
    pg_private::dispatch<T2>::store(s2, t2),
    pg_private::dispatch<T3>::store(s3, t3),
    pg_private::dispatch<T4>::store(s4, t4),
    pg_private::dispatch<T5>::store(s5, t5),
  };
  const int paramLengths[] = {
    pg_private::dispatch<T1>::length(t1),
    pg_private::dispatch<T2>::length(t2),
    pg_private::dispatch<T3>::length(t3),
    pg_private::dispatch<T4>::length(t4),
    pg_private::dispatch<T5>::length(t5),
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

template <class T1, class T2, class T3, class T4, class T5, class T6>
inline void
pg_query(pgconn_handle &conn, pgresult_handle &res, const char *command,
	 const T1 &t1, const T2 &t2, const T3 &t3, const T4 &t4, const T5 &t5,
	 const T6 &t6)
{
  char s1[pg_private::dispatch<T1>::storage];
  char s2[pg_private::dispatch<T2>::storage];
  char s3[pg_private::dispatch<T3>::storage];
  char s4[pg_private::dispatch<T4>::storage];
  char s5[pg_private::dispatch<T5>::storage];
  char s6[pg_private::dispatch<T6>::storage];
  const Oid paramTypes[] = {
    pg_private::dispatch<T1>::oid,
    pg_private::dispatch<T2>::oid,
    pg_private::dispatch<T3>::oid,
    pg_private::dispatch<T4>::oid,
    pg_private::dispatch<T5>::oid,
    pg_private::dispatch<T6>::oid,
  };
  const char * paramValues[] = {
    pg_private::dispatch<T1>::store(s1, t1),
    pg_private::dispatch<T2>::store(s2, t2),
    pg_private::dispatch<T3>::store(s3, t3),
    pg_private::dispatch<T4>::store(s4, t4),
    pg_private::dispatch<T5>::store(s5, t5),
    pg_private::dispatch<T6>::store(s6, t6),
  };
  const int paramLengths[] = {
    pg_private::dispatch<T1>::length(t1),
    pg_private::dispatch<T2>::length(t2),
    pg_private::dispatch<T3>::length(t3),
    pg_private::dispatch<T4>::length(t4),
    pg_private::dispatch<T5>::length(t5),
    pg_private::dispatch<T6>::length(t6),
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
	  class T7> inline void
pg_query(pgconn_handle &conn, pgresult_handle &res, const char *command,
	 const T1 &t1, const T2 &t2, const T3 &t3, const T4 &t4, const T5 &t5,
	 const T6 &t6, const T7 &t7)
{
  char s1[pg_private::dispatch<T1>::storage];
  char s2[pg_private::dispatch<T2>::storage];
  char s3[pg_private::dispatch<T3>::storage];
  char s4[pg_private::dispatch<T4>::storage];
  char s5[pg_private::dispatch<T5>::storage];
  char s6[pg_private::dispatch<T6>::storage];
  char s7[pg_private::dispatch<T7>::storage];
  const Oid paramTypes[] = {
    pg_private::dispatch<T1>::oid,
    pg_private::dispatch<T2>::oid,
    pg_private::dispatch<T3>::oid,
    pg_private::dispatch<T4>::oid,
    pg_private::dispatch<T5>::oid,
    pg_private::dispatch<T6>::oid,
    pg_private::dispatch<T7>::oid,
  };
  const char * paramValues[] = {
    pg_private::dispatch<T1>::store(s1, t1),
    pg_private::dispatch<T2>::store(s2, t2),
    pg_private::dispatch<T3>::store(s3, t3),
    pg_private::dispatch<T4>::store(s4, t4),
    pg_private::dispatch<T5>::store(s5, t5),
    pg_private::dispatch<T6>::store(s6, t6),
    pg_private::dispatch<T7>::store(s7, t7),
  };
  const int paramLengths[] = {
    pg_private::dispatch<T1>::length(t1),
    pg_private::dispatch<T2>::length(t2),
    pg_private::dispatch<T3>::length(t3),
    pg_private::dispatch<T4>::length(t4),
    pg_private::dispatch<T5>::length(t5),
    pg_private::dispatch<T6>::length(t6),
    pg_private::dispatch<T7>::length(t7),
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
	  class T7, class T8> inline void
pg_query(pgconn_handle &conn, pgresult_handle &res, const char *command,
	 const T1 &t1, const T2 &t2, const T3 &t3, const T4 &t4, const T5 &t5,
	 const T6 &t6, const T7 &t7, const T8 &t8)
{
  char s1[pg_private::dispatch<T1>::storage];
  char s2[pg_private::dispatch<T2>::storage];
  char s3[pg_private::dispatch<T3>::storage];
  char s4[pg_private::dispatch<T4>::storage];
  char s5[pg_private::dispatch<T5>::storage];
  char s6[pg_private::dispatch<T6>::storage];
  char s7[pg_private::dispatch<T7>::storage];
  char s8[pg_private::dispatch<T8>::storage];
  const Oid paramTypes[] = {
    pg_private::dispatch<T1>::oid,
    pg_private::dispatch<T2>::oid,
    pg_private::dispatch<T3>::oid,
    pg_private::dispatch<T4>::oid,
    pg_private::dispatch<T5>::oid,
    pg_private::dispatch<T6>::oid,
    pg_private::dispatch<T7>::oid,
    pg_private::dispatch<T8>::oid,
  };
  const char * paramValues[] = {
    pg_private::dispatch<T1>::store(s1, t1),
    pg_private::dispatch<T2>::store(s2, t2),
    pg_private::dispatch<T3>::store(s3, t3),
    pg_private::dispatch<T4>::store(s4, t4),
    pg_private::dispatch<T5>::store(s5, t5),
    pg_private::dispatch<T6>::store(s6, t6),
    pg_private::dispatch<T7>::store(s7, t7),
    pg_private::dispatch<T8>::store(s8, t8),
  };
  const int paramLengths[] = {
    pg_private::dispatch<T1>::length(t1),
    pg_private::dispatch<T2>::length(t2),
    pg_private::dispatch<T3>::length(t3),
    pg_private::dispatch<T4>::length(t4),
    pg_private::dispatch<T5>::length(t5),
    pg_private::dispatch<T6>::length(t6),
    pg_private::dispatch<T7>::length(t7),
    pg_private::dispatch<T8>::length(t8),
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
	  class T7, class T8, class T9> inline void
pg_query(pgconn_handle &conn, pgresult_handle &res, const char *command,
	 const T1 &t1, const T2 &t2, const T3 &t3, const T4 &t4, const T5 &t5,
	 const T6 &t6, const T7 &t7, const T8 &t8, const T9 &t9)
{
  char s1[pg_private::dispatch<T1>::storage];
  char s2[pg_private::dispatch<T2>::storage];
  char s3[pg_private::dispatch<T3>::storage];
  char s4[pg_private::dispatch<T4>::storage];
  char s5[pg_private::dispatch<T5>::storage];
  char s6[pg_private::dispatch<T6>::storage];
  char s7[pg_private::dispatch<T7>::storage];
  char s8[pg_private::dispatch<T8>::storage];
  char s9[pg_private::dispatch<T9>::storage];
  const Oid paramTypes[] = {
    pg_private::dispatch<T1>::oid,
    pg_private::dispatch<T2>::oid,
    pg_private::dispatch<T3>::oid,
    pg_private::dispatch<T4>::oid,
    pg_private::dispatch<T5>::oid,
    pg_private::dispatch<T6>::oid,
    pg_private::dispatch<T7>::oid,
    pg_private::dispatch<T8>::oid,
    pg_private::dispatch<T9>::oid,
  };
  const char * paramValues[] = {
    pg_private::dispatch<T1>::store(s1, t1),
    pg_private::dispatch<T2>::store(s2, t2),
    pg_private::dispatch<T3>::store(s3, t3),
    pg_private::dispatch<T4>::store(s4, t4),
    pg_private::dispatch<T5>::store(s5, t5),
    pg_private::dispatch<T6>::store(s6, t6),
    pg_private::dispatch<T7>::store(s7, t7),
    pg_private::dispatch<T8>::store(s8, t8),
    pg_private::dispatch<T9>::store(s9, t9),
  };
  const int paramLengths[] = {
    pg_private::dispatch<T1>::length(t1),
    pg_private::dispatch<T2>::length(t2),
    pg_private::dispatch<T3>::length(t3),
    pg_private::dispatch<T4>::length(t4),
    pg_private::dispatch<T5>::length(t5),
    pg_private::dispatch<T6>::length(t6),
    pg_private::dispatch<T7>::length(t8),
    pg_private::dispatch<T8>::length(t8),
    pg_private::dispatch<T9>::length(t9),
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

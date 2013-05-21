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

namespace cxxll {

// Not for direct use.
namespace pg_private {

  template <class T1> void
  pg_query(int fmt, pgconn_handle &conn, pgresult_handle &res, const char *sql,
	   typename dispatch<T1>::arg t1)
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
    res.execParamsCustom
      (conn, sql, 1, paramTypes, paramValues, paramLengths, paramFormats, fmt);
  }

  template <class T1, class T2> void
  pg_query(int fmt, pgconn_handle &conn, pgresult_handle &res, const char *sql,
	   typename dispatch<T1>::arg t1, typename dispatch<T2>::arg t2)
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
    res.execParamsCustom
      (conn, sql, 2, paramTypes, paramValues, paramLengths, paramFormats, fmt);
  }

  template <class T1, class T2, class T3> void
  pg_query(int fmt, pgconn_handle &conn, pgresult_handle &res, const char *sql,
	   typename dispatch<T1>::arg t1, typename dispatch<T2>::arg t2,
	   typename dispatch<T3>::arg t3)
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
    res.execParamsCustom
      (conn, sql, 3, paramTypes, paramValues, paramLengths, paramFormats, fmt);
  }

  template <class T1, class T2, class T3, class T4> void
  pg_query(int fmt, pgconn_handle &conn, pgresult_handle &res, const char *sql,
	   typename dispatch<T1>::arg t1, typename dispatch<T2>::arg t2,
	   typename dispatch<T3>::arg t3, typename dispatch<T4>::arg t4)
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
    res.execParamsCustom
      (conn, sql, 4, paramTypes, paramValues, paramLengths, paramFormats, fmt);
  }

  template <class T1, class T2, class T3, class T4, class T5> void
  pg_query(int fmt, pgconn_handle &conn, pgresult_handle &res, const char *sql,
	   typename dispatch<T1>::arg t1, typename dispatch<T2>::arg t2,
	   typename dispatch<T3>::arg t3, typename dispatch<T4>::arg t4,
	   typename dispatch<T5>::arg t5)
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
    res.execParamsCustom
      (conn, sql, 5, paramTypes, paramValues, paramLengths, paramFormats, fmt);
  }

  template <class T1, class T2, class T3, class T4, class T5, class T6>
  void
  pg_query(int fmt, pgconn_handle &conn, pgresult_handle &res, const char *sql,
	   typename dispatch<T1>::arg t1, typename dispatch<T2>::arg t2,
	   typename dispatch<T3>::arg t3, typename dispatch<T4>::arg t4,
	   typename dispatch<T5>::arg t5, typename dispatch<T6>::arg t6)
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
    res.execParamsCustom
      (conn, sql, 6, paramTypes, paramValues, paramLengths, paramFormats, fmt);
  }

  template <class T1, class T2, class T3, class T4, class T5, class T6,
	    class T7> void
  pg_query(int fmt, pgconn_handle &conn, pgresult_handle &res, const char *sql,
	   typename dispatch<T1>::arg t1, typename dispatch<T2>::arg t2,
	   typename dispatch<T3>::arg t3, typename dispatch<T4>::arg t4,
	   typename dispatch<T5>::arg t5, typename dispatch<T6>::arg t6,
	   typename dispatch<T7>::arg t7)
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
    res.execParamsCustom
      (conn, sql, 7, paramTypes, paramValues, paramLengths, paramFormats, fmt);
  }

  template <class T1, class T2, class T3, class T4, class T5, class T6,
	    class T7, class T8> void
  pg_query(int fmt, pgconn_handle &conn, pgresult_handle &res, const char *sql,
	   typename dispatch<T1>::arg t1, typename dispatch<T2>::arg t2,
	   typename dispatch<T3>::arg t3, typename dispatch<T4>::arg t4,
	   typename dispatch<T5>::arg t5, typename dispatch<T6>::arg t6,
	   typename dispatch<T7>::arg t7, typename dispatch<T8>::arg t8)
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
    res.execParamsCustom
      (conn, sql, 8, paramTypes, paramValues, paramLengths, paramFormats, fmt);
  }

  template <class T1, class T2, class T3, class T4, class T5, class T6,
	    class T7, class T8, class T9> void
  pg_query(int fmt, pgconn_handle &conn, pgresult_handle &res, const char *sql,
	   typename dispatch<T1>::arg t1, typename dispatch<T2>::arg t2,
	   typename dispatch<T3>::arg t3, typename dispatch<T4>::arg t4,
	   typename dispatch<T5>::arg t5, typename dispatch<T6>::arg t6,
	   typename dispatch<T7>::arg t7, typename dispatch<T8>::arg t8,
	   typename dispatch<T9>::arg t9)
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
      pg_private::dispatch<T7>::length(t7),
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
    res.execParamsCustom
      (conn, sql, 9, paramTypes, paramValues, paramLengths, paramFormats, fmt);
  }

  template <class T1, class T2, class T3, class T4, class T5, class T6,
	    class T7, class T8, class T9, class T10> void
  pg_query(int fmt, pgconn_handle &conn, pgresult_handle &res, const char *sql,
	   typename dispatch<T1>::arg t1, typename dispatch<T2>::arg t2,
	   typename dispatch<T3>::arg t3, typename dispatch<T4>::arg t4,
	   typename dispatch<T5>::arg t5, typename dispatch<T6>::arg t6,
	   typename dispatch<T7>::arg t7, typename dispatch<T8>::arg t8,
	   typename dispatch<T9>::arg t9, typename dispatch<T10>::arg t10)
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
    char s10[pg_private::dispatch<T10>::storage];
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
      pg_private::dispatch<T10>::oid,
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
      pg_private::dispatch<T10>::store(s10, t10),
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
      pg_private::dispatch<T9>::length(t9),
      pg_private::dispatch<T10>::length(t10),
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
      1,
    };
    res.execParamsCustom
      (conn, sql, 10, paramTypes, paramValues, paramLengths, paramFormats, fmt);
  }

  template <class T1, class T2, class T3, class T4, class T5, class T6,
	    class T7, class T8, class T9, class T10, class T11> void
  pg_query(int fmt, pgconn_handle &conn, pgresult_handle &res, const char *sql,
	   typename dispatch<T1>::arg t1, typename dispatch<T2>::arg t2,
	   typename dispatch<T3>::arg t3, typename dispatch<T4>::arg t4,
	   typename dispatch<T5>::arg t5, typename dispatch<T6>::arg t6,
	   typename dispatch<T7>::arg t7, typename dispatch<T8>::arg t8,
	   typename dispatch<T9>::arg t9, typename dispatch<T10>::arg t10,
	   typename dispatch<T11>::arg t11)
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
    char s10[pg_private::dispatch<T10>::storage];
    char s11[pg_private::dispatch<T11>::storage];
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
      pg_private::dispatch<T10>::oid,
      pg_private::dispatch<T11>::oid,
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
      pg_private::dispatch<T10>::store(s10, t10),
      pg_private::dispatch<T11>::store(s11, t11),
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
      pg_private::dispatch<T9>::length(t9),
      pg_private::dispatch<T10>::length(t10),
      pg_private::dispatch<T11>::length(t11),
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
      1,
      1,
    };
    res.execParamsCustom
      (conn, sql, 11, paramTypes, paramValues, paramLengths, paramFormats, fmt);
  }

  template <class T1, class T2, class T3, class T4, class T5, class T6,
	    class T7, class T8, class T9, class T10, class T11, class T12> void
  pg_query(int fmt, pgconn_handle &conn, pgresult_handle &res, const char *sql,
	   typename dispatch<T1>::arg t1, typename dispatch<T2>::arg t2,
	   typename dispatch<T3>::arg t3, typename dispatch<T4>::arg t4,
	   typename dispatch<T5>::arg t5, typename dispatch<T6>::arg t6,
	   typename dispatch<T7>::arg t7, typename dispatch<T8>::arg t8,
	   typename dispatch<T9>::arg t9, typename dispatch<T10>::arg t10,
	   typename dispatch<T11>::arg t11, typename dispatch<T12>::arg t12)
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
    char s10[pg_private::dispatch<T10>::storage];
    char s11[pg_private::dispatch<T11>::storage];
    char s12[pg_private::dispatch<T12>::storage];
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
      pg_private::dispatch<T10>::oid,
      pg_private::dispatch<T11>::oid,
      pg_private::dispatch<T12>::oid,
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
      pg_private::dispatch<T10>::store(s10, t10),
      pg_private::dispatch<T11>::store(s11, t11),
      pg_private::dispatch<T12>::store(s12, t12),
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
      pg_private::dispatch<T9>::length(t9),
      pg_private::dispatch<T10>::length(t10),
      pg_private::dispatch<T11>::length(t11),
      pg_private::dispatch<T12>::length(t12),
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
      1,
      1,
      1,
    };
    res.execParamsCustom
      (conn, sql, 12, paramTypes, paramValues, paramLengths, paramFormats, fmt);
  }

  template <class T1, class T2, class T3, class T4, class T5, class T6,
	    class T7, class T8, class T9, class T10, class T11, class T12,
	    class T13> void
  pg_query(int fmt, pgconn_handle &conn, pgresult_handle &res, const char *sql,
	   typename dispatch<T1>::arg t1, typename dispatch<T2>::arg t2,
	   typename dispatch<T3>::arg t3, typename dispatch<T4>::arg t4,
	   typename dispatch<T5>::arg t5, typename dispatch<T6>::arg t6,
	   typename dispatch<T7>::arg t7, typename dispatch<T8>::arg t8,
	   typename dispatch<T9>::arg t9, typename dispatch<T10>::arg t10,
	   typename dispatch<T11>::arg t11, typename dispatch<T12>::arg t12,
	   typename dispatch<T13>::arg t13)
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
    char s10[pg_private::dispatch<T10>::storage];
    char s11[pg_private::dispatch<T11>::storage];
    char s12[pg_private::dispatch<T12>::storage];
    char s13[pg_private::dispatch<T13>::storage];
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
      pg_private::dispatch<T10>::oid,
      pg_private::dispatch<T11>::oid,
      pg_private::dispatch<T12>::oid,
      pg_private::dispatch<T13>::oid,
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
      pg_private::dispatch<T10>::store(s10, t10),
      pg_private::dispatch<T11>::store(s11, t11),
      pg_private::dispatch<T12>::store(s12, t12),
      pg_private::dispatch<T13>::store(s13, t13),
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
      pg_private::dispatch<T9>::length(t9),
      pg_private::dispatch<T10>::length(t10),
      pg_private::dispatch<T11>::length(t11),
      pg_private::dispatch<T12>::length(t12),
      pg_private::dispatch<T13>::length(t13),
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
      1,
      1,
      1,
      1,
    };
    res.execParamsCustom
      (conn, sql, 13, paramTypes, paramValues, paramLengths, paramFormats, fmt);
  }

  template <class T1, class T2, class T3, class T4, class T5, class T6,
	    class T7, class T8, class T9, class T10, class T11, class T12,
	    class T13, class T14> void
  pg_query(int fmt, pgconn_handle &conn, pgresult_handle &res, const char *sql,
	   typename dispatch<T1>::arg t1, typename dispatch<T2>::arg t2,
	   typename dispatch<T3>::arg t3, typename dispatch<T4>::arg t4,
	   typename dispatch<T5>::arg t5, typename dispatch<T6>::arg t6,
	   typename dispatch<T7>::arg t7, typename dispatch<T8>::arg t8,
	   typename dispatch<T9>::arg t9, typename dispatch<T10>::arg t10,
	   typename dispatch<T11>::arg t11, typename dispatch<T12>::arg t12,
	   typename dispatch<T13>::arg t13, typename dispatch<T14>::arg t14)
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
    char s10[pg_private::dispatch<T10>::storage];
    char s11[pg_private::dispatch<T11>::storage];
    char s12[pg_private::dispatch<T12>::storage];
    char s13[pg_private::dispatch<T13>::storage];
    char s14[pg_private::dispatch<T14>::storage];
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
      pg_private::dispatch<T10>::oid,
      pg_private::dispatch<T11>::oid,
      pg_private::dispatch<T12>::oid,
      pg_private::dispatch<T13>::oid,
      pg_private::dispatch<T14>::oid,
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
      pg_private::dispatch<T10>::store(s10, t10),
      pg_private::dispatch<T11>::store(s11, t11),
      pg_private::dispatch<T12>::store(s12, t12),
      pg_private::dispatch<T13>::store(s13, t13),
      pg_private::dispatch<T14>::store(s14, t14),
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
      pg_private::dispatch<T9>::length(t9),
      pg_private::dispatch<T10>::length(t10),
      pg_private::dispatch<T11>::length(t11),
      pg_private::dispatch<T12>::length(t12),
      pg_private::dispatch<T13>::length(t13),
      pg_private::dispatch<T14>::length(t14),
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
      1,
      1,
      1,
      1,
      1,
    };
    res.execParamsCustom
      (conn, sql, 14, paramTypes, paramValues, paramLengths, paramFormats, fmt);
  }

  template <class T1, class T2, class T3, class T4, class T5, class T6,
	    class T7, class T8, class T9, class T10, class T11, class T12,
	    class T13, class T14, class T15> void
  pg_query(int fmt, pgconn_handle &conn, pgresult_handle &res, const char *sql,
	   typename dispatch<T1>::arg t1, typename dispatch<T2>::arg t2,
	   typename dispatch<T3>::arg t3, typename dispatch<T4>::arg t4,
	   typename dispatch<T5>::arg t5, typename dispatch<T6>::arg t6,
	   typename dispatch<T7>::arg t7, typename dispatch<T8>::arg t8,
	   typename dispatch<T9>::arg t9, typename dispatch<T10>::arg t10,
	   typename dispatch<T11>::arg t11, typename dispatch<T12>::arg t12,
	   typename dispatch<T13>::arg t13, typename dispatch<T14>::arg t14,
	   typename dispatch<T15>::arg t15)
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
    char s10[pg_private::dispatch<T10>::storage];
    char s11[pg_private::dispatch<T11>::storage];
    char s12[pg_private::dispatch<T12>::storage];
    char s13[pg_private::dispatch<T13>::storage];
    char s14[pg_private::dispatch<T14>::storage];
    char s15[pg_private::dispatch<T15>::storage];
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
      pg_private::dispatch<T10>::oid,
      pg_private::dispatch<T11>::oid,
      pg_private::dispatch<T12>::oid,
      pg_private::dispatch<T13>::oid,
      pg_private::dispatch<T14>::oid,
      pg_private::dispatch<T15>::oid,
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
      pg_private::dispatch<T10>::store(s10, t10),
      pg_private::dispatch<T11>::store(s11, t11),
      pg_private::dispatch<T12>::store(s12, t12),
      pg_private::dispatch<T13>::store(s13, t13),
      pg_private::dispatch<T14>::store(s14, t14),
      pg_private::dispatch<T15>::store(s15, t15),
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
      pg_private::dispatch<T9>::length(t9),
      pg_private::dispatch<T10>::length(t10),
      pg_private::dispatch<T11>::length(t11),
      pg_private::dispatch<T12>::length(t12),
      pg_private::dispatch<T13>::length(t13),
      pg_private::dispatch<T14>::length(t14),
      pg_private::dispatch<T15>::length(t15),
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
      1,
      1,
      1,
      1,
      1,
      1,
    };
    res.execParamsCustom
      (conn, sql, 15, paramTypes, paramValues, paramLengths, paramFormats, fmt);
  }

} // pg_private

template <class T1> inline void
pg_query(pgconn_handle &conn, pgresult_handle &res, const char *sql,
	 const T1 &t1)
{
  return pg_private::pg_query<T1>(0, conn, res, sql, t1);
}

template <class T1>
inline void
pg_query_binary(pgconn_handle &conn, pgresult_handle &res, const char *sql,
		const T1 &t1)
{
  return pg_private::pg_query<T1>(1, conn, res, sql, t1);
}

template <class T1, class T2> inline void
pg_query(pgconn_handle &conn, pgresult_handle &res, const char *sql,
	 const T1 &t1, const T2 &t2)
{
  return pg_private::pg_query<T1, T2>(0, conn, res, sql, t1, t2);
}

template <class T1, class T2>
inline void
pg_query_binary(pgconn_handle &conn, pgresult_handle &res, const char *sql,
		const T1 &t1, const T2 &t2)
{
  return pg_private::pg_query<T1, T2>(1, conn, res, sql, t1, t2);
}

template <class T1, class T2, class T3> inline void
pg_query(pgconn_handle &conn, pgresult_handle &res, const char *sql,
	 const T1 &t1, const T2 &t2, const T3 &t3)
{
  return pg_private::pg_query<T1, T2, T3>(0, conn, res, sql, t1, t2, t3);
}

template <class T1, class T2, class T3>
inline void
pg_query_binary(pgconn_handle &conn, pgresult_handle &res, const char *sql,
		const T1 &t1, const T2 &t2, const T3 &t3)
{
  return pg_private::pg_query<T1, T2, T3>(1, conn, res, sql, t1, t2, t3);
}

template <class T1, class T2, class T3, class T4> inline void
pg_query(pgconn_handle &conn, pgresult_handle &res, const char *sql,
	 const T1 &t1, const T2 &t2, const T3 &t3, const T4 &t4)
{
  return pg_private::pg_query<T1, T2, T3, T4>
    (0, conn, res, sql, t1, t2, t3, t4);
}

template <class T1, class T2, class T3, class T4>
inline void
pg_query_binary(pgconn_handle &conn, pgresult_handle &res, const char *sql,
		const T1 &t1, const T2 &t2, const T3 &t3, const T4 &t4)
{
  return pg_private::pg_query<T1, T2, T3, T4>
    (1, conn, res, sql, t1, t2, t3, t4);
}

template <class T1, class T2, class T3, class T4, class T5> inline void
pg_query(pgconn_handle &conn, pgresult_handle &res, const char *sql,
	 const T1 &t1, const T2 &t2, const T3 &t3, const T4 &t4,
	 const T5 &t5)
{
  return pg_private::pg_query<T1, T2, T3, T4, T5>
    (0, conn, res, sql, t1, t2, t3, t4, t5);
}

template <class T1, class T2, class T3, class T4, class T5>
inline void
pg_query_binary(pgconn_handle &conn, pgresult_handle &res, const char *sql,
		const T1 &t1, const T2 &t2, const T3 &t3, const T4 &t4,
		const T5 &t5)
{
  return pg_private::pg_query<T1, T2, T3, T4, T5>
    (1, conn, res, sql, t1, t2, t3, t4, t5);
}

template <class T1, class T2, class T3, class T4, class T5, class T6>
inline void
pg_query(pgconn_handle &conn, pgresult_handle &res, const char *sql,
	 const T1 &t1, const T2 &t2, const T3 &t3, const T4 &t4,
	 const T5 &t5, const T6 &t6)
{
  return pg_private::pg_query<T1, T2, T3, T4, T5, T6>
    (0, conn, res, sql, t1, t2, t3, t4, t5, t6);
}

template <class T1, class T2, class T3, class T4, class T5, class T6>
inline void
pg_query_binary(pgconn_handle &conn, pgresult_handle &res, const char *sql,
		const T1 &t1, const T2 &t2, const T3 &t3, const T4 &t4,
		const T5 &t5, const T6 &t6)
{
  return pg_private::pg_query<T1, T2, T3, T4, T5, T6>
    (1, conn, res, sql, t1, t2, t3, t4, t5, t6);
}

template <class T1, class T2, class T3, class T4, class T5, class T6,
	  class T7> inline void
pg_query(pgconn_handle &conn, pgresult_handle &res, const char *sql,
	 const T1 &t1, const T2 &t2, const T3 &t3, const T4 &t4,
	 const T5 &t5, const T6 &t6, const T7 &t7)
{
  return pg_private::pg_query<T1, T2, T3, T4, T5, T6, T7>
    (0, conn, res, sql, t1, t2, t3, t4, t5, t6, t7);
}

template <class T1, class T2, class T3, class T4, class T5, class T6,
	  class T7> inline void
pg_query_binary(pgconn_handle &conn, pgresult_handle &res, const char *sql,
		const T1 &t1, const T2 &t2, const T3 &t3, const T4 &t4,
		const T5 &t5, const T6 &t6, const T7 &t7)
{
  return pg_private::pg_query<T1, T2, T3, T4, T5, T6, T7>
    (1, conn, res, sql, t1, t2, t3, t4, t5, t6, t7);
}

template <class T1, class T2, class T3, class T4, class T5, class T6,
	  class T7, class T8> inline void
pg_query(pgconn_handle &conn, pgresult_handle &res, const char *sql,
	 const T1 &t1, const T2 &t2, const T3 &t3, const T4 &t4,
	 const T5 &t5, const T6 &t6, const T7 &t7, const T8 &t8)
{
  return pg_private::pg_query<T1, T2, T3, T4, T5, T6, T7, T8>
    (0, conn, res, sql, t1, t2, t3, t4, t5, t6, t7, t8);
}

template <class T1, class T2, class T3, class T4, class T5, class T6,
	  class T7, class T8> inline void
pg_query_binary(pgconn_handle &conn, pgresult_handle &res, const char *sql,
		const T1 &t1, const T2 &t2, const T3 &t3, const T4 &t4,
		const T5 &t5, const T6 &t6, const T7 &t7, const T8 &t8)
{
  return pg_private::pg_query<T1, T2, T3, T4, T5, T6, T7, T8>
    (1, conn, res, sql, t1, t2, t3, t4, t5, t6, t7, t8);
}

template <class T1, class T2, class T3, class T4, class T5, class T6,
	  class T7, class T8, class T9> inline void
pg_query(pgconn_handle &conn, pgresult_handle &res, const char *sql,
	 const T1 &t1, const T2 &t2, const T3 &t3, const T4 &t4,
	 const T5 &t5, const T6 &t6, const T7 &t7, const T8 &t8,
	 const T9 &t9)
{
  return pg_private::pg_query<T1, T2, T3, T4, T5, T6, T7, T8, T9>
    (0, conn, res, sql, t1, t2, t3, t4, t5, t6, t7, t8, t9);
}

template <class T1, class T2, class T3, class T4, class T5, class T6,
	  class T7, class T8, class T9> inline void
pg_query_binary(pgconn_handle &conn, pgresult_handle &res, const char *sql,
		const T1 &t1, const T2 &t2, const T3 &t3, const T4 &t4,
		const T5 &t5, const T6 &t6, const T7 &t7, const T8 &t8,
		const T9 &t9)
{
  return pg_private::pg_query<T1, T2, T3, T4, T5, T6, T7, T8, T9>
    (1, conn, res, sql, t1, t2, t3, t4, t5, t6, t7, t8, t9);
}

template <class T1, class T2, class T3, class T4, class T5, class T6,
	  class T7, class T8, class T9, class T10> inline void
pg_query(pgconn_handle &conn, pgresult_handle &res, const char *sql,
	 const T1 &t1, const T2 &t2, const T3 &t3, const T4 &t4,
	 const T5 &t5, const T6 &t6, const T7 &t7, const T8 &t8,
	 const T9 &t9, const T10 &t10)
{
  return pg_private::pg_query<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10>
    (0, conn, res, sql, t1, t2, t3, t4, t5, t6, t7, t8, t9, t10);
}

template <class T1, class T2, class T3, class T4, class T5, class T6,
	  class T7, class T8, class T9, class T10> inline void
pg_query_binary(pgconn_handle &conn, pgresult_handle &res, const char *sql,
		const T1 &t1, const T2 &t2, const T3 &t3, const T4 &t4,
		const T5 &t5, const T6 &t6, const T7 &t7, const T8 &t8,
		const T9 &t9, const T10 &t10)
{
  return pg_private::pg_query<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10>
    (1, conn, res, sql, t1, t2, t3, t4, t5, t6, t7, t8, t9, t10);
}

template <class T1, class T2, class T3, class T4, class T5, class T6,
	  class T7, class T8, class T9, class T10, class T11> inline void
pg_query(pgconn_handle &conn, pgresult_handle &res, const char *sql,
	 const T1 &t1, const T2 &t2, const T3 &t3, const T4 &t4,
	 const T5 &t5, const T6 &t6, const T7 &t7, const T8 &t8,
	 const T9 &t9, const T10 &t10, const T11 &t11)
{
  return pg_private::pg_query<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11>
    (0, conn, res, sql, t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11);
}

template <class T1, class T2, class T3, class T4, class T5, class T6,
	  class T7, class T8, class T9, class T10, class T11> inline void
pg_query_binary(pgconn_handle &conn, pgresult_handle &res, const char *sql,
		const T1 &t1, const T2 &t2, const T3 &t3, const T4 &t4,
		const T5 &t5, const T6 &t6, const T7 &t7, const T8 &t8,
		const T9 &t9, const T10 &t10, const T11 &t11)
{
  return pg_private::pg_query<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11>
    (1, conn, res, sql, t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11);
}

template <class T1, class T2, class T3, class T4, class T5, class T6,
	  class T7, class T8, class T9, class T10, class T11,
	  class T12> inline void
pg_query(pgconn_handle &conn, pgresult_handle &res, const char *sql,
	 const T1 &t1, const T2 &t2, const T3 &t3, const T4 &t4,
	 const T5 &t5, const T6 &t6, const T7 &t7, const T8 &t8,
	 const T9 &t9, const T10 &t10, const T11 &t11, const T12 &t12)
{
  return pg_private::pg_query<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11,
			      T12>
    (0, conn, res, sql, t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12);
}

template <class T1, class T2, class T3, class T4, class T5, class T6,
	  class T7, class T8, class T9, class T10, class T11,
	  class T12> inline void
pg_query_binary(pgconn_handle &conn, pgresult_handle &res, const char *sql,
		const T1 &t1, const T2 &t2, const T3 &t3, const T4 &t4,
		const T5 &t5, const T6 &t6, const T7 &t7, const T8 &t8,
		const T9 &t9, const T10 &t10, const T11 &t11, const T12 &t12)
{
  return pg_private::pg_query<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11,
			      T12>
    (1, conn, res, sql, t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12);
}

template <class T1, class T2, class T3, class T4, class T5, class T6,
	  class T7, class T8, class T9, class T10, class T11,
	  class T12, class T13> inline void
pg_query(pgconn_handle &conn, pgresult_handle &res, const char *sql,
	 const T1 &t1, const T2 &t2, const T3 &t3, const T4 &t4,
	 const T5 &t5, const T6 &t6, const T7 &t7, const T8 &t8,
	 const T9 &t9, const T10 &t10, const T11 &t11, const T12 &t12,
	 const T13 &t13)
{
  return pg_private::pg_query<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11,
			      T12, T13>
    (0, conn, res, sql, t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12,
     t13);
}

template <class T1, class T2, class T3, class T4, class T5, class T6,
	  class T7, class T8, class T9, class T10, class T11,
	  class T12, class T13> inline void
pg_query_binary(pgconn_handle &conn, pgresult_handle &res, const char *sql,
		const T1 &t1, const T2 &t2, const T3 &t3, const T4 &t4,
		const T5 &t5, const T6 &t6, const T7 &t7, const T8 &t8,
		const T9 &t9, const T10 &t10, const T11 &t11, const T12 &t12,
		const T13 &t13)
{
  return pg_private::pg_query<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11,
			      T12, T13>
    (1, conn, res, sql, t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12,
     t13);
}

template <class T1, class T2, class T3, class T4, class T5, class T6,
	  class T7, class T8, class T9, class T10, class T11,
	  class T12, class T13, class T14> inline void
pg_query(pgconn_handle &conn, pgresult_handle &res, const char *sql,
	 const T1 &t1, const T2 &t2, const T3 &t3, const T4 &t4,
	 const T5 &t5, const T6 &t6, const T7 &t7, const T8 &t8,
	 const T9 &t9, const T10 &t10, const T11 &t11, const T12 &t12,
	 const T13 &t13, const T14 &t14)
{
  return pg_private::pg_query<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11,
			      T12, T13, T14>
    (0, conn, res, sql, t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12,
     t13, t14);
}

template <class T1, class T2, class T3, class T4, class T5, class T6,
	  class T7, class T8, class T9, class T10, class T11,
	  class T12, class T13, class T14> inline void
pg_query_binary(pgconn_handle &conn, pgresult_handle &res, const char *sql,
		const T1 &t1, const T2 &t2, const T3 &t3, const T4 &t4,
		const T5 &t5, const T6 &t6, const T7 &t7, const T8 &t8,
		const T9 &t9, const T10 &t10, const T11 &t11, const T12 &t12,
		const T13 &t13, const T14 &t14)
{
  return pg_private::pg_query<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11,
			      T12, T13, T14>
    (1, conn, res, sql, t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12,
     t13, t14);
}

template <class T1, class T2, class T3, class T4, class T5, class T6,
	  class T7, class T8, class T9, class T10, class T11,
	  class T12, class T13, class T14, class T15> inline void
pg_query(pgconn_handle &conn, pgresult_handle &res, const char *sql,
	 const T1 &t1, const T2 &t2, const T3 &t3, const T4 &t4,
	 const T5 &t5, const T6 &t6, const T7 &t7, const T8 &t8,
	 const T9 &t9, const T10 &t10, const T11 &t11, const T12 &t12,
	 const T13 &t13, const T14 &t14, const T15 &t15)
{
  return pg_private::pg_query<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11,
			      T12, T13, T14, T15>
    (0, conn, res, sql, t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12,
     t13, t14, t15);
}

template <class T1, class T2, class T3, class T4, class T5, class T6,
	  class T7, class T8, class T9, class T10, class T11,
	  class T12, class T13, class T14, class T15> inline void
pg_query_binary(pgconn_handle &conn, pgresult_handle &res, const char *sql,
		const T1 &t1, const T2 &t2, const T3 &t3, const T4 &t4,
		const T5 &t5, const T6 &t6, const T7 &t7, const T8 &t8,
		const T9 &t9, const T10 &t10, const T11 &t11, const T12 &t12,
		const T13 &t13, const T14 &t14, const T15 &t15)
{
  return pg_private::pg_query<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11,
			      T12, T13, T14, T15>
    (1, conn, res, sql, t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12,
     t13, t14, t15);
}

} // namespace cxxll

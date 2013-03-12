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
pg_response(pgresult_handle &res, int row, T1 &t1)
{
  PGresult *r = res.get();
  pg_private::dispatch<T1>::load(r, 0, row, t1);
}

template <class T1, class T2> inline void
pg_response(pgresult_handle &res, int row, T1 &t1, T2 &t2)
{
  PGresult *r = res.get();
  pg_private::dispatch<T1>::load(r, row, 0, t1);
  pg_private::dispatch<T1>::load(r, row, 1, t2);
}

template <class T1, class T2, class T3> inline void
pg_response(pgresult_handle &res, int row, T1 &t1, T2 &t2, T3 &t3)
{
  PGresult *r = res.get();
  pg_private::dispatch<T1>::load(r, row, 0, t1);
  pg_private::dispatch<T1>::load(r, row, 1, t2);
  pg_private::dispatch<T1>::load(r, row, 2, t3);
}

template <class T1, class T2, class T3, class T4> inline void
pg_response(pgresult_handle &res, int row, T1 &t1, T2 &t2, T3 &t3, T4 &t4)
{
  PGresult *r = res.get();
  pg_private::dispatch<T1>::load(r, row, 0, t1);
  pg_private::dispatch<T1>::load(r, row, 1, t2);
  pg_private::dispatch<T1>::load(r, row, 2, t3);
  pg_private::dispatch<T1>::load(r, row, 3, t4);
}

template <class T1, class T2, class T3, class T4, class T5> inline void
pg_response(pgresult_handle &res, int row, T1 &t1, T2 &t2, T3 &t3, T4 &t4,
	    T5 &t5)
{
  PGresult *r = res.get();
  pg_private::dispatch<T1>::load(r, row, 0, t1);
  pg_private::dispatch<T1>::load(r, row, 1, t2);
  pg_private::dispatch<T1>::load(r, row, 2, t3);
  pg_private::dispatch<T1>::load(r, row, 3, t4);
  pg_private::dispatch<T1>::load(r, row, 4, t5);
}

template <class T1, class T2, class T3, class T4, class T5, class T6>
inline void
pg_response(pgresult_handle &res, int row, T1 &t1, T2 &t2, T3 &t3, T4 &t4,
	    T5 &t5, T6 &t6)
{
  PGresult *r = res.get();
  pg_private::dispatch<T1>::load(r, row, 0, t1);
  pg_private::dispatch<T1>::load(r, row, 1, t2);
  pg_private::dispatch<T1>::load(r, row, 2, t3);
  pg_private::dispatch<T1>::load(r, row, 3, t4);
  pg_private::dispatch<T1>::load(r, row, 4, t5);
  pg_private::dispatch<T1>::load(r, row, 5, t6);
}

template <class T1, class T2, class T3, class T4, class T5, class T6, class T7>
inline void
pg_response(pgresult_handle &res, int row, T1 &t1, T2 &t2, T3 &t3, T4 &t4,
	    T5 &t5, T6 &t6, T7 &t7)
{
  PGresult *r = res.get();
  pg_private::dispatch<T1>::load(r, row, 0, t1);
  pg_private::dispatch<T1>::load(r, row, 1, t2);
  pg_private::dispatch<T1>::load(r, row, 2, t3);
  pg_private::dispatch<T1>::load(r, row, 3, t4);
  pg_private::dispatch<T1>::load(r, row, 4, t5);
  pg_private::dispatch<T1>::load(r, row, 5, t6);
  pg_private::dispatch<T1>::load(r, row, 6, t7);
}

template <class T1, class T2, class T3, class T4, class T5, class T6, class T7,
	  class T8> inline void
pg_response(pgresult_handle &res, int row, T1 &t1, T2 &t2, T3 &t3, T4 &t4,
	    T5 &t5, T6 &t6, T7 &t7, T8 &t8)
{
  PGresult *r = res.get();
  pg_private::dispatch<T1>::load(r, row, 0, t1);
  pg_private::dispatch<T1>::load(r, row, 1, t2);
  pg_private::dispatch<T1>::load(r, row, 2, t3);
  pg_private::dispatch<T1>::load(r, row, 3, t4);
  pg_private::dispatch<T1>::load(r, row, 4, t5);
  pg_private::dispatch<T1>::load(r, row, 5, t6);
  pg_private::dispatch<T1>::load(r, row, 6, t7);
  pg_private::dispatch<T1>::load(r, row, 7, t8);
}

template <class T1, class T2, class T3, class T4, class T5, class T6, class T7,
	  class T8, class T9> inline void
pg_response(pgresult_handle &res, int row, T1 &t1, T2 &t2, T3 &t3, T4 &t4,
	    T5 &t5, T6 &t6, T7 &t7, T8 &t8, T9 &t9)
{
  PGresult *r = res.get();
  pg_private::dispatch<T1>::load(r, row, 0, t1);
  pg_private::dispatch<T1>::load(r, row, 1, t2);
  pg_private::dispatch<T1>::load(r, row, 2, t3);
  pg_private::dispatch<T1>::load(r, row, 3, t4);
  pg_private::dispatch<T1>::load(r, row, 4, t5);
  pg_private::dispatch<T1>::load(r, row, 5, t6);
  pg_private::dispatch<T1>::load(r, row, 6, t7);
  pg_private::dispatch<T1>::load(r, row, 7, t8);
  pg_private::dispatch<T1>::load(r, row, 8, t9);
}

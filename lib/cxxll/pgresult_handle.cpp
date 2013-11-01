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

#include <cxxll/pgresult_handle.hpp>
#include <cxxll/pgconn_handle.hpp>
#include <cxxll/pg_exception.hpp>

#include "symboldb_config.h"

using namespace cxxll;

static void
do_check(PGresult *raw)
{
  if (raw == nullptr) {
    throw pg_exception(raw);
  }
  switch (PQresultStatus(raw)) {
  case PGRES_EMPTY_QUERY:
  case PGRES_COMMAND_OK:
  case PGRES_TUPLES_OK:
  case PGRES_COPY_OUT:
  case PGRES_COPY_IN:
  case PGRES_COPY_BOTH:
#ifdef HAVE_PG_SINGLE_TUPLE
  case PGRES_SINGLE_TUPLE:
#endif
    return;
  case PGRES_BAD_RESPONSE:
  case PGRES_NONFATAL_ERROR:
  case PGRES_FATAL_ERROR:
    throw pg_exception(raw);
  }
}

pgresult_handle::pgresult_handle(PGresult *newraw)
{
  try {
    do_check(newraw);
  } catch (...) {
    PQclear(newraw);
    throw;
  }
  raw = newraw;
}

void
pgresult_handle::check()
{
  do_check(raw);
}

void
pgresult_handle::reset(PGresult *newraw)
{
  try {
    do_check(newraw);
  } catch (...) {
    PQclear(newraw);
    throw;
  }
  PQclear(raw);
  raw = newraw;
}

void
pgresult_handle::getresult(pgconn_handle &h)
{
  reset(PQgetResult(h.get()));
}

void
pgresult_handle::exec(pgconn_handle &conn, const char *command)
{
  PGresult *newraw = PQexec(conn.get(), command);
  reset(newraw);
}

void
pgresult_handle::execParamsCustom(pgconn_handle &conn,
			const char *command,
			int nParams,
			const Oid *paramTypes,
			const char *const * paramValues,
			const int *paramLengths,
			const int *paramFormats,
			int resultFormat)
{
  PGresult *newraw = PQexecParams(conn.get(), command, nParams, paramTypes,
				  paramValues, paramLengths, paramFormats,
				  resultFormat);
  reset(newraw);
}

void
pgresult_handle::execBinary(pgconn_handle &conn, const char *command)
{
  execParamsCustom(conn, command, 0, nullptr, nullptr, nullptr, nullptr, 1);
}

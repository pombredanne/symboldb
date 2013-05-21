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

#include <libpq-fe.h>

namespace cxxll {

class pgconn_handle;

// Wrapper around a PostgreSQL result object (PGresult).
class pgresult_handle {
  pgresult_handle(const pgresult_handle &); // not implemented
  pgresult_handle &operator=(const pgresult_handle &); // not implemented
  PGresult *raw;
public:

  // Initializes the raw pointer to NULL.
  pgresult_handle() throw();

  // Initializes the raw pointer with PGRESULT, taking ownership.
  // Throws pg_exception on error, freeing the result.
  explicit pgresult_handle(PGresult *);

  // Deallocates the raw pointer (if it is not NULL).
  ~pgresult_handle() throw();

  // Throws pg_exception if the raw pointer is NULL or in an error
  // state.
  void check();

  // Returns the raw pointer.
  PGresult *get() throw();

  // Returns the connection handle and sets the raw pointer to NULL,
  // releasing ownership of the handle.
  PGresult *release() throw();

  // Replaces the raw pointer with PGRESULT, closing it first if
  // necessary.  Throws pg_exception on error, freeing the new result
  // (and preserving the old one).
  void reset(PGresult *);

  // Calls PQgetresult and resets the raw pointer on success.  Throws
  // pg_exception on error.
  void getresult(pgconn_handle &);

  // Closes the connection handle and sets the raw pointer to NULL.
  void close() throw();

  // Calls PQntuples().
  int ntuples() const throw();

  // Calls PQgetvalue().  Counting of rows and columns starts at 0.
  const char *getvalue(int row, int column) const throw();

   // Calls PQgetlength().  Counting of rows and columns starts at 0.
  int getlength(int row, int column) const throw();

  // Calls PQgetisnull().  Counting of rows and columns starts at 0.
  bool getisnull(int row, int column) const throw();

  // Calls PQresultStatus().
  ExecStatusType resultStatus() const throw();

  // Calls PQexec().  Throws pg_exception on error.  Uses text mode
  // for the output.
  void exec(pgconn_handle &, const char *command);

  // Calls PQexec().  Throws pg_exception on error.  Uses binary mode
  // for the output.
  void execBinary(pgconn_handle &, const char *command);

  // Calls PQexecParam().  Throws pg_exception on error.  Uses text
  // mode for the output.
  template <unsigned N>
  void execParams(pgconn_handle &,
		  const char *command, const char *(&paramValues)[N]);

  // Calls PQexecParam().  Throws pg_exception on error.  Uses binary
  // mode for the output.
  template <unsigned N>
  void execParamsBinary(pgconn_handle &,
			const char *command, const char *(&paramValues)[N]);

  // Calls PQexecParam().  Throws pg_exception on error.  Uses text
  // mode for the output.
  template <unsigned N>
  void execTypedParams(pgconn_handle &,
		       const char *command,
		       const Oid (&paramTypes)[N],
		       const char *(&paramValues)[N],
		       const int (&paramLengths)[N],
		       const int (&paramFormats)[N]);

  // Calls PQexecParam().  Throws pg_exception on error.  Uses binary
  // mode for the output.
  template <unsigned N>
  void execTypedParamsBinary(pgconn_handle &,
			     const char *command,
			     const Oid (&paramTypes)[N],
			     const char *(&paramValues)[N],
			     const int (&paramLengths)[N],
			     const int (&paramFormats)[N]);

  // Calls PQexecParam().  Throws pg_exception on error.
  void execParamsCustom(pgconn_handle &,
			const char *command,
			int nParams,
			const Oid *paramTypes,
			const char *const * paramValues,
			const int *paramLengths,
			const int *paramFormats,
			int resultFormat);
};

inline
pgresult_handle::pgresult_handle() throw()
  : raw(NULL)
{
}

inline
pgresult_handle::~pgresult_handle() throw()
{
  PQclear(raw);
}

inline PGresult *
pgresult_handle::get() throw ()
{
  return raw;
}

inline PGresult *
pgresult_handle::release() throw()
{
  PGresult *c = raw;
  raw = NULL;
  return c;
}

inline void
pgresult_handle::close() throw()
{
  PQclear(raw);
  raw = NULL;
}

inline int
pgresult_handle::ntuples() const throw ()
{
  return PQntuples(raw);
}

inline const char *
pgresult_handle::getvalue(int row, int column) const throw ()
{
  return PQgetvalue(raw, row, column);
}

inline int
pgresult_handle::getlength(int row, int column) const throw ()
{
  return PQgetlength(raw, row, column);
}

inline bool
pgresult_handle::getisnull(int row, int column) const throw ()
{
  return PQgetisnull(raw, row, column);
}

inline ExecStatusType
pgresult_handle::resultStatus() const throw ()
{
  return PQresultStatus(raw);
}

template <unsigned N> void inline
pgresult_handle::execTypedParams(pgconn_handle &conn,
		  const char *command,
		  const Oid (&paramTypes)[N],
		  const char *(&paramValues)[N],
		  const int (&paramLengths)[N],
		  const int (&paramFormats)[N])
{
  execParamsCustom(conn, command, N,
		   paramTypes, paramValues, paramLengths, paramFormats, 0);
}

template <unsigned N> void inline
pgresult_handle::execTypedParamsBinary(pgconn_handle &conn,
		  const char *command,
		  const Oid (&paramTypes)[N],
		  const char *(&paramValues)[N],
		  const int (&paramLengths)[N],
		  const int (&paramFormats)[N])
{
  execParamsCustom(conn, command, N,
		   paramTypes, paramValues, paramLengths, paramFormats, 1);
}

template <unsigned N> void
pgresult_handle::execParams(pgconn_handle &conn,
			   const char *command, const char *(&paramValues)[N])
{
  execParamsCustom(conn, command, N, NULL, paramValues, NULL, NULL, 0);
}

template <unsigned N> void
pgresult_handle::execParamsBinary(pgconn_handle &conn,
			   const char *command, const char *(&paramValues)[N])
{
  execParamsCustom(conn, command, N, NULL, paramValues, NULL, NULL, 1);
}

} // namespace cxxll

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

// Wrapper around a PostgreSQL result object (PGresult).
class pgresult_handle {
  pgresult_handle(const pgresult_handle &); // not implemented
  pgresult_handle &operator=(const pgresult_handle &); // not implemented
public:
  PGresult *raw;

  // Initializes RAW with NULL.
  pgresult_handle() throw();

  // Initiales RAW with PGRESULT, taking ownership.
  explicit pgresult_handle(PGresult *) throw();

  // Deallocates RAW if it is not NULL.
  ~pgresult_handle() throw();

  // Returns the connection handle and sets RAW to NULL, releasing
  // ownership of the handle.
  PGresult *release() throw();

  // Closes the connection handle and sets RAW to NULL.
  void close() throw();

  // Throws pg_exception if RAW is NULL or in an error state.
  void check();
};

inline
pgresult_handle::pgresult_handle() throw()
  : raw(NULL)
{
}

inline
pgresult_handle::pgresult_handle(PGresult *c) throw()
  : raw(c)
{
}

inline
pgresult_handle::~pgresult_handle() throw()
{
  PQclear(raw);
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


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

// Wrapper around a PostgreSQL connection object (PGconn).
class pgconn_handle {
  pgconn_handle(const pgconn_handle &); // not implemented
  pgconn_handle &operator=(const pgconn_handle &); // not implemented
public:
  PGconn *raw;

  // Initializes RAW with NULL.
  pgconn_handle() throw();

  // Initiales RAW with PGCONN, taking ownership.
  explicit pgconn_handle(PGconn *) throw();

  // Deallocates RAW if it is not NULL.
  ~pgconn_handle() throw();

  // Returns the connection handle and sets RAW to NULL, releasing
  // ownership of the handle.
  PGconn *release() throw();

  // Replaces RAW with PGCONN, closing RAW first if necessary.
  void reset(PGconn *) throw();

  // Closes the connection handle and sets RAW to NULL.
  void close() throw();

  // Throws pg_exception if RAW is NULL or in an error state.
  void check();
};

inline
pgconn_handle::pgconn_handle() throw()
  : raw(NULL)
{
}

inline
pgconn_handle::pgconn_handle(PGconn *c) throw()
  : raw(c)
{
}

inline
pgconn_handle::~pgconn_handle() throw()
{
  PQfinish(raw);
}

inline PGconn *
pgconn_handle::release() throw()
{
  PGconn *c = raw;
  raw = NULL;
  return c;
}

inline void
pgconn_handle::reset(PGconn *conn) throw()
{
  PQfinish(raw);
  raw = conn;
}

inline void
pgconn_handle::close() throw()
{
  PQfinish(raw);
  raw = NULL;
}

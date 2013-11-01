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

#include <string>

#include <libpq-fe.h>

namespace cxxll {

// Wrapper around a PostgreSQL connection object (PGconn).
class pgconn_handle {
  pgconn_handle(const pgconn_handle &) = delete;
  pgconn_handle &operator=(const pgconn_handle &) = delete;
  PGconn *raw;
public:
  // Initializes the raw pointer with NULL.
  pgconn_handle() throw();

  // Initiales the raw pointer with PGCONN, taking ownership.  Throws
  // pg_exception and frees the connection if it reflects an error
  // value.
  explicit pgconn_handle(PGconn *);

  // Deallocates the raw pointer if it is not NULL.
  ~pgconn_handle() throw();

  // Returns the raw pointer.
  PGconn *get() throw();

  // Returns the connection handle and sets the raw pointer to NULL,
  // releasing ownership of the handle.
  PGconn *release() throw();

  // Replaces the raw pointer with PGCONN, closing the raw pointer
  // first if necessary.  Throws pg_exception and frees the new
  // connection if it reflects an error, retaining the old one.
  void reset(PGconn *);

  // Closes the connection handle and sets the raw pointer to NULL.
  void close() throw();

  // Calls PQtransactionStatus
  PGTransactionStatusType transactionStatus() const;

  // Calls PQputCopyData().  Throws pg_exception on error.
  void putCopyData(const char *buffer, size_t nbytes);

  // Calls PQputCopyEnd().  Throws pg_exception on error.  Use
  // pgresult_handle::getresult() to obtain the server status.
  void putCopyEnd();

  // Calls PQputCopyEnd().  Throws pg_exception on error.  Use
  // pgresult_handle::getresult() to obtain the server status.
  void putCopyEndError(const char *errormsg);

  // Calls PQgetCopyData() and replaces the string contents with the
  // data provided.  If no more data is available returns false.
  // Throws pg_exception on error.
  bool getCopyData(std::string &row);

  // Throws pg_exception if the raw pointer is NULL or in an error
  // state.
  void check();
};

inline
pgconn_handle::pgconn_handle() throw()
  : raw(nullptr)
{
}

inline
pgconn_handle::~pgconn_handle() throw()
{
  PQfinish(raw);
}

inline PGconn *
pgconn_handle::get() throw()
{
  return raw;
}

inline PGconn *
pgconn_handle::release() throw()
{
  PGconn *c = raw;
  raw = nullptr;
  return c;
}

inline void
pgconn_handle::close() throw()
{
  PQfinish(raw);
  raw = nullptr;
}

inline PGTransactionStatusType
pgconn_handle::transactionStatus() const
{
  return PQtransactionStatus(raw);
}

} // namespace cxxll

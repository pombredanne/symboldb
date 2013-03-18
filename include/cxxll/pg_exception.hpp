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

#include <cstdio>
#include <stdexcept>
#include <string>


// Captures information related to a PostgreSQL error.
class pg_exception : public std::exception {
public:
  // Minimal error message.
  explicit pg_exception(const char *);

  // Minimal error message from the connection.
  explicit pg_exception(PGconn *);

  // Initializes the exception from the result.
  explicit pg_exception(PGresult *);

  ~pg_exception() throw();

  // Empty strings were not present in the error message.
  // Missing integers are assigned -1.

  std::string message_; // from PQresultErrorMessage()
  ExecStatusType status_; // from PGresultStatus()
  std::string severity_; // PG_DIAG_SEVERITY
  std::string sqlstate_; // PG_DIAG_SQLSTATE
  std::string primary_; // PG_DIAG_MESSAGE_PRIMARY
  std::string detail_; // PG_DIAG_MESSAGE_DETAIL
  std::string hint_; // PG_DIAG_MESSAGE_HINT
  std::string internal_query_; // PG_DIAG_INTERNAL_QUERY
  std::string context_; // PG_DIAG_SOURCE_FILE
  std::string source_file_; // PG_DIAG_SOURCE_FILE
  std::string source_function_; // PG_DIAG_SOURCE_FUNCTION
  int statement_position_; // PG_DIAG_STATEMENT_POSITION
  int internal_position_; // PG_DIAG_INTERNAL_POSITION
  int source_line_; // PG_DIAG_SOURCE_LINE

  // Returns the message from PQresultErrorMessage().
  const char *what() const throw();
};

// Writes a description of the exception to the stream.
void dump(const char *prefix, const pg_exception &, FILE *);

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

#include <cxxll/pg_exception.hpp>

#include <string.h>

namespace {
  void
  set_field_string(PGresult *res, std::string &field, int code)
  {
    const char *p = PQresultErrorField(res, code);
    if (p != NULL) {
      field = p;
    }
  }

  void
  set_field_int(PGresult *res, int &field, int code)
  {
    const char *p = PQresultErrorField(res, code);
    if (p != NULL) {
      sscanf(p, "%d", &field);
    }
  }
}

pg_exception::pg_exception(const char *message)
  : statement_position_(-1), internal_position_(-1), source_line_(-1)
{
  status_ = PGRES_FATAL_ERROR;
  message_ = message;
  severity_ = "FATAL";
  sqlstate_ = "58000";
}

pg_exception::pg_exception(PGconn *conn)
  : statement_position_(-1), internal_position_(-1), source_line_(-1)
{
  status_ = PGRES_FATAL_ERROR;
  if (conn == NULL) {
    message_ = "out of memory";
  } else {
    message_ = PQerrorMessage(conn);
  }
  severity_ = "FATAL";
  sqlstate_ = "58000";
}

pg_exception::pg_exception(PGresult *res)
  : statement_position_(-1), internal_position_(-1), source_line_(-1)
{
  if (res == NULL) {
    status_ = PGRES_FATAL_ERROR;
    message_ = "out of memory";
    severity_ = "FATAL";
    sqlstate_ = "53200";
  } else {
    status_ = PQresultStatus(res);
    message_ = PQresultErrorMessage(res);
    set_field_string(res, severity_, PG_DIAG_SEVERITY);
    set_field_string(res, sqlstate_, PG_DIAG_SQLSTATE);
    set_field_string(res, primary_, PG_DIAG_MESSAGE_PRIMARY);
    set_field_string(res, detail_, PG_DIAG_MESSAGE_DETAIL);
    set_field_string(res, hint_, PG_DIAG_MESSAGE_HINT);
    set_field_string(res, internal_query_, PG_DIAG_INTERNAL_QUERY);
    set_field_string(res, context_, PG_DIAG_SOURCE_FILE);
    set_field_string(res, source_file_, PG_DIAG_SOURCE_FILE);
    set_field_string(res, source_function_, PG_DIAG_SOURCE_FUNCTION);
    set_field_int(res, statement_position_, PG_DIAG_STATEMENT_POSITION);
    set_field_int(res, internal_position_, PG_DIAG_INTERNAL_POSITION);
    set_field_int(res, source_line_, PG_DIAG_SOURCE_LINE);
  }
}

pg_exception::~pg_exception() throw()
{
}

const char *
pg_exception::what() const throw()
{
  return message_.c_str();
}

namespace {
  void
  dump(const char *prefix, const char *infix1, const char *infix2,
       const char *message, FILE *out)
  {
    bool first = true;
    while (true) {
      const char *nl = strchr(message, '\n');
      if (nl == NULL) {
	nl = message + strlen(message);
      }
      if (*message == '\0') {
	break;
      }
      fprintf(out, "%s%s", prefix, first ? infix1 : infix2);
      fwrite(message, nl - message, 1, out);
      putc('\n', out);
      first = false;
      message = nl;
      if (*message == '\n') {
	++message;
      }
    }
  }

  void
  dump(const char *prefix, const char *infix1, const char *infix2,
       const std::string &message, FILE *out)
  {
    if (!message.empty()) {
      dump(prefix, infix1, infix2, message.c_str(), out);
    }
  }
}

void
dump(const char *prefix, const pg_exception &e, FILE *out)
{
  dump(prefix, "", "  ", e.message_, out);
  fprintf(out, "%s  status=%s severity=%s sqlstate=%s", prefix,
	  PQresStatus(e.status_), e.severity_.c_str(), e.sqlstate_.c_str());
  if (e.statement_position_ >= 0) {
    fprintf(out,  " position=%d\n", e.statement_position_);
  } else {
    putc('\n', out);
  }
  dump(prefix, "  message: ", "message2: ", e.primary_, out);
  dump(prefix, "  detail: ", "detail: ", e.detail_, out);
  dump(prefix, "  hint: ", "hint: ", e.detail_, out);
  dump(prefix, "  internal: ", "internal: ", e.internal_query_, out);
  if (e.internal_position_ >= 0) {
    fprintf(out, "%s  internal position: %d\n", prefix, e.internal_position_);
  }
  if (e.context_ != e.source_file_) {
    dump(prefix, "  context: ", "context: ", e.context_, out);
  }
  if (!e.source_file_.empty() && e.source_line_ >= 0) {
    fprintf(out, "%s  location: %s:%d%s%s\n", prefix,
	    e.source_file_.c_str(), e.source_line_,
	    e.source_function_.empty() ? "" : " ",
	    e.source_function_.c_str());
  }
}

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

#include <cxxll/pg_query.hpp>
#include <cxxll/pg_exception.hpp>
#include <cxxll/string_support.hpp>
#include <cxxll/raise.hpp>

#include <cerrno>
#include <climits>
#include <cstdlib>

#include <sstream>

using namespace cxxll;

const Oid pg_private::dispatch<short>::oid;
const int pg_private::dispatch<short>::storage;

const Oid pg_private::dispatch<int>::oid;
const int pg_private::dispatch<int>::storage;

const Oid pg_private::dispatch<long long>::oid;
const int pg_private::dispatch<long long>::storage;

const Oid pg_private::dispatch<const char *>::oid;
const int pg_private::dispatch<const char *>::storage;

const Oid pg_private::dispatch<std::string>::oid;
const int pg_private::dispatch<std::string>::storage;

int
pg_private::length_check(size_t len)
{
  if (len >= INT_MAX) {
    throw pg_exception("argument string length exceeds maximum");
  }
  return len;
}

static inline bool
is_binary(PGresult *res, int col)
{
  switch (PQfformat(res, col)) {
  case 0:
    return false;
    break;
  case 1:
    return true;
    break;
  default:
    throw pg_exception("invalid format type");
  }
}

// Throws pg_exception after appending " column COL of row ROW" to the
// message.
static void throw_mismatch_exception(const char *msg, int row, int col)
  __attribute__((noreturn));
static void
throw_mismatch_exception(const char *msg, int row, int col)
{
    std::ostringstream str;
    str << msg << " column " << col << " of row " << row;
    throw pg_exception(str.str());
}

void
pg_private::dispatch<bool>::load(PGresult *res, int row, int col,
				       bool &val)
{
  bool binary = is_binary(res, col);
  if (binary && (PQftype(res, col) != oid
		 || PQgetlength(res, row, col) != storage)) {
    throw pg_exception("format mismatch for boolean column");
  }
  if (PQgetisnull(res, row, col)) {
    throw_mismatch_exception("NULL value in non-null boolean", row, col);
  }
  const char *ptr =PQgetvalue(res, row, col);
  if (binary) {
    switch (*ptr) {
    case 0:
      val = false;
      break;
    case 1:
      val = true;
      break;
    default:
      throw pg_exception("invalid binary value in boolean column");
    }
  } else {
    switch (*ptr) {
    case 'f':
      val = false;
      break;
    case 't':
      val = true;
      break;
    default:
      throw pg_exception("invalid value in boolean column");
    }
  }
}


template <class T> static inline void
load_integer(PGresult *res, int row, int col, T &val)
{
  using namespace pg_private;
  bool binary = is_binary(res, col);
  if (binary && (PQftype(res, col) != dispatch<T>::oid
		 || PQgetlength(res, row, col) != dispatch<T>::storage)) {
    throw_mismatch_exception("format mismatch for integer", row, col);
  }
  if (PQgetisnull(res, row, col)) {
    throw_mismatch_exception("NULL value in non-null integer", row, col);
  }
  if (binary) {
    const unsigned char *ptr =
      reinterpret_cast<const unsigned char *>(PQgetvalue(res, row, col));
    if (sizeof(T) == 2) {
      val = (static_cast<T>(ptr[0]) << 8) | ptr[1];
    } else if (sizeof(T) == 4) {
      memcpy(&val, ptr, dispatch<T>::storage);
      val = be_to_cpu_32(val);
    } else if (sizeof(T) == 8) {
      memcpy(&val, ptr, dispatch<T>::storage);
      val = be_to_cpu_64(val);
    }
  } else {
    long long llval;
    char *endptr;
    errno = 0;
    const char *ptr = PQgetvalue(res, row, col);
    llval = strtoll(ptr, &endptr, 10);
    if ((llval == 0 && errno != 0)
	|| static_cast<long long>(static_cast<T>(llval)) != llval) {
      std::string msg("conversion failure for INT");
      msg += static_cast<char>('0' + sizeof(T));
      msg += " column: \"";
      msg += quote(ptr);
      msg += '"';
      throw pg_exception(msg.c_str());
    }
    val = llval;
  }
}

void
pg_private::dispatch<short>::load(PGresult *res, int row, int col,
					short &val)
{
  load_integer<short>(res, row, col, val);
}

void
pg_private::dispatch<int>::load(PGresult *res, int row, int col, int &val)
{
  load_integer<int>(res, row, col, val);
}

void
pg_private::dispatch<long long>::load(PGresult *res, int row, int col,
					    long long &val)
{
  load_integer<long long>(res, row, col, val);
}

template <class T>
static inline void
load_text(PGresult *res, int row, int col, T &val)
{
  using namespace pg_private;
  bool binary = is_binary(res, col);
  Oid colid = PQftype(res, col);
  bool bytea = colid == dispatch<std::vector<unsigned char> >::oid;
  if (binary && (colid != dispatch<std::string>::oid && !bytea)) {
    throw pg_exception("format mismatch for string column");
  }
  const char *ptr = PQgetvalue(res, row, col);
  if (binary || !bytea) {
    val.assign(ptr, ptr + PQgetlength(res, row, col));
  } else {
    size_t len;
    unsigned char *blob =
      PQunescapeBytea(reinterpret_cast<const unsigned char *>(ptr), &len);
    if (blob == NULL) {
      raise<std::bad_alloc>();
    }
    try {
      val.assign(blob, blob + len);
    } catch (...) {
      PQfreemem(blob);
      throw;
    }
    PQfreemem(blob);
  }
}

void
pg_private::dispatch<std::string>::load(PGresult *res, int row, int col,
					      std::string &val)
{
  load_text<std::string>(res, row, col, val);
}

void
pg_private::dispatch<std::vector<unsigned char> >::load
  (PGresult *res, int row, int col, std::vector<unsigned char> &val)
{
  load_text<std::vector<unsigned char> >(res, row, col, val);
}

int
pg_private::dispatch<std::string>::length(const std::string &str)
{
  return length_check(str.size());
}


const char *
pg_private::dispatch<std::vector<unsigned char> >::store
  (char *, const std::vector<unsigned char> &vec)
{
  if (vec.empty()) {
    // vec.data() could be a null pointer, which would be interpreted
    // by PostgreSQL as a NULL value.
    return "";
  } else {
    return reinterpret_cast<const char *>(vec.data());
  }
}

int
pg_private::dispatch<std::vector<unsigned char> >::length
  (const std::vector<unsigned char> &vec)
{
  return length_check(vec.size());
}

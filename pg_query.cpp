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

#include "pg_query.hpp"
#include "pg_exception.hpp"

#include <climits>

const Oid pg_query_private::dispatch<short>::oid;
const unsigned pg_query_private::dispatch<short>::storage;

const Oid pg_query_private::dispatch<int>::oid;
const unsigned pg_query_private::dispatch<int>::storage;

const Oid pg_query_private::dispatch<long long>::oid;
const unsigned pg_query_private::dispatch<long long>::storage;

const Oid pg_query_private::dispatch<const char *>::oid;
const unsigned pg_query_private::dispatch<const char *>::storage;

const Oid pg_query_private::dispatch<std::string>::oid;
const unsigned pg_query_private::dispatch<std::string>::storage;

int
pg_query_private::length_check(size_t len)
{
  if (len >= INT_MAX) {
    throw pg_exception("argument string length exceeds maximum");
  }
  return len;
}

int
pg_query_private::dispatch<std::string>::length(const std::string &str)
{
  return length_check(str.size());
}


const char *
pg_query_private::dispatch<std::vector<unsigned char> >::store
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
pg_query_private::dispatch<std::vector<unsigned char> >::length
  (const std::vector<unsigned char> &vec)
{
  return length_check(vec.size());
}

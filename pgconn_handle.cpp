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

#include "pgconn_handle.hpp"
#include "pg_exception.hpp"

#include <algorithm>
#include <climits>

static void
do_check(PGconn *raw)
{
  if (raw == NULL || PQstatus(raw) != CONNECTION_OK) {
    throw pg_exception(raw);
  }
}

pgconn_handle::pgconn_handle(PGconn *c)
{
  do_check(c);
  raw = c;
}

void
pgconn_handle::reset(PGconn *c)
{
  do_check(c);
  close();
  raw = c;
}

void
pgconn_handle::putCopyData(const char *p, size_t len)
{
  while (len) {
    size_t to_copy = std::min(len, static_cast<size_t>(INT_MAX));
    int result = PQputCopyData(raw, p, to_copy);
    if (result < 0) {
      throw pg_exception(raw);
    }
    p += to_copy;
    len -= to_copy;
  }
}

void
pgconn_handle::putCopyEnd()
{
  putCopyEndError(NULL);
}

void
pgconn_handle::putCopyEndError(const char *errormsg)
{
  int result = PQputCopyEnd(raw, errormsg);
  if (result < 0) {
    throw pg_exception(raw);
  }
}

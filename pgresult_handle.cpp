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

#include "pgresult_handle.hpp"
#include "pg_exception.hpp"

#include "symboldb_config.h"

void
pgresult_handle::check()
{
  if (raw == NULL) {
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

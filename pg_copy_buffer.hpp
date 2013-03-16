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
#include <tr1/memory>
#include <vector>

class pgconn_handle;

// Buffer for storing COPY FROM STDIN data.
class pg_copy_buffer {
  struct impl;
  std::tr1::shared_ptr<impl> impl_;
public:
  // Creates a buffer for the specified database handle, with the
  // specified command.  (The command may be executed multiple times
  // if flush() is called.)  Does not take ownership of the handle.
  pg_copy_buffer(pgconn_handle *, const char *command);
  ~pg_copy_buffer();

  pg_copy_buffer &column(bool);
  pg_copy_buffer &column(short);
  pg_copy_buffer &column(int);
  pg_copy_buffer &column(long long);
  pg_copy_buffer &column(const char *);
  pg_copy_buffer &column(const std::string &);
  pg_copy_buffer &column(const std::vector<unsigned char> &);

  // Ends the current row.  Checks that the number of columns matches the 
  void end();

  // Terminates the current COPY FROM operation.  After this call, and
  // up to the next call to one of the column() functions, the
  // PostgreSQL connection handle can be used for other queries.
  void flush();

  // Totally empties the buffer and sends all data over the
  // connection.  This has to be called before committing the
  // transaction.
  void flush_all();

  // Discards all pending data and terminates the copying operation.
  void discard();
};

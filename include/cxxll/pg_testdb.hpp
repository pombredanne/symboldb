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

#include <string>
#include <tr1/memory>
#include <vector>

namespace cxxll {

// A wrapper for a PostgreSQL server running as a subprocess, against
// a temporary database directory (which is removed when the object is
// destroyed).
class pg_testdb {
  struct impl;
  std::tr1::shared_ptr<impl> impl_;
public:
  pg_testdb();
  ~pg_testdb();

  // Notices are suppressed and added to this vector.
  const std::vector<std::string> &notices() const;

  // The path to the database directory.
  const std::string &directory();

  // The path to the log file in the database directory.
  const std::string &logfile();

  // Connects to the database with the indicated name;
  PGconn *connect(const char *dbname);

  // Runs the specified SQL string in the database.  It is recommended
  // to use this function for dangerous commands (DROP SCHEMA etc.),
  // to make sure that this isn't issue against a production database
  // by accident.
  void exec_test_sql(const char *dbname, const char *sql);

  // Writes out the collected log information.  Useful in case of
  // errors.
  void dump_logs();
};

} // namespace cxxll

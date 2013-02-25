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

#include "rpm_load.hpp"
#include "rpm_package_info.hpp"

#include "database.hpp"
#include "dir_handle.hpp"
#include "pg_testdb.hpp"
#include "pgconn_handle.hpp"
#include "pgresult_handle.hpp"
#include "string_support.hpp"
#include "symboldb_options.hpp"

#include "test.hpp"

static void
test()
{
  static const char DBNAME[] = "template1";
  pg_testdb testdb;
  {
    // Run this directly, to suppress notices.
    pgconn_handle db(testdb.connect(DBNAME));
    db.check();
    pgresult_handle res(PQexec(db.raw, database::SCHEMA));
    res.check();
  }

  symboldb_options opt;
  opt.output = symboldb_options::quiet;
  database db(testdb.directory().c_str(), DBNAME);

  static const char RPMDIR[] = "test/data";
  std::string rpmdir_prefix(RPMDIR);
  rpmdir_prefix += '/';
  dir_handle rpmdir(RPMDIR);
  while (dirent *e = rpmdir.readdir()) {
    if (ends_with(std::string(e->d_name), ".rpm")
        && !ends_with(std::string(e->d_name), ".src.rpm")) {
      rpm_package_info info;
      rpm_load(opt, db, (rpmdir_prefix + e->d_name).c_str(), info);
    }
  }
}

static test_register t("rpm_load", test);

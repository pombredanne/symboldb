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
    pgresult_handle res(PQexec(db.raw, database::SCHEMA));
    res.check();
  }

  symboldb_options opt;
  opt.output = symboldb_options::quiet;
  database db(testdb.directory().c_str(), DBNAME);

  {
    database::package_id last_pkg_id(0);
   static const char RPMDIR[] = "test/data";
    std::string rpmdir_prefix(RPMDIR);
    rpmdir_prefix += '/';
    dir_handle rpmdir(RPMDIR);
    while (dirent *e = rpmdir.readdir()) {
      if (ends_with(std::string(e->d_name), ".rpm")
	  && !ends_with(std::string(e->d_name), ".src.rpm")) {
	rpm_package_info info;
	database::package_id pkg
	  (rpm_load(opt, db, (rpmdir_prefix + e->d_name).c_str(), info));
	CHECK(pkg > last_pkg_id);
	last_pkg_id = pkg;
	pkg = rpm_load(opt, db, (rpmdir_prefix + e->d_name).c_str(), info);
	CHECK(pkg == last_pkg_id);
      }
    }
  }

  {
    static const char *const sysvinit_files_6[] = {
      "/sbin/killall5",
      "/sbin/pidof",
      "/sbin/sulogin",
      "/usr/bin/last",
      "/usr/bin/lastb",
      "/usr/bin/mesg",
      "/usr/bin/utmpdump",
      "/usr/bin/wall",
      "/usr/share/doc/sysvinit-tools-2.88",
      "/usr/share/doc/sysvinit-tools-2.88/Changelog",
      "/usr/share/doc/sysvinit-tools-2.88/COPYRIGHT",
      "/usr/share/man/man1/last.1.gz",
      "/usr/share/man/man1/lastb.1.gz",
      "/usr/share/man/man1/mesg.1.gz",
      "/usr/share/man/man1/utmpdump.1.gz",
      "/usr/share/man/man1/wall.1.gz",
      "/usr/share/man/man8/killall5.8.gz",
      "/usr/share/man/man8/pidof.8.gz",
      "/usr/share/man/man8/sulogin.8.gz",
      NULL
    };
    static const char *const sysvinit_files_9[] = {
      "/sbin/killall5",
      "/sbin/pidof",
      "/usr/bin/last",
      "/usr/bin/lastb",
      "/usr/bin/mesg",
      "/usr/bin/wall",
      "/usr/share/doc/sysvinit-tools-2.88",
      "/usr/share/doc/sysvinit-tools-2.88/Changelog",
      "/usr/share/doc/sysvinit-tools-2.88/COPYRIGHT",
      "/usr/share/man/man1/last.1.gz",
      "/usr/share/man/man1/lastb.1.gz",
      "/usr/share/man/man1/mesg.1.gz",
      "/usr/share/man/man1/wall.1.gz",
      "/usr/share/man/man8/killall5.8.gz",
      "/usr/share/man/man8/pidof.8.gz",
      NULL
    };

    pgconn_handle dbh(testdb.connect(DBNAME));
    std::vector<database::package_id> pids;
    pgresult_handle r1
      (PQexec(dbh.raw,
	      "SELECT id, name, version, release FROM symboldb.package"));
    r1.check();
    for (int i = 0, endi = PQntuples(r1.raw); i < endi; ++i) {
      {
	int pkg = 0;
	sscanf(PQgetvalue(r1.raw, i, 0), "%d", &pkg);
	CHECK(pkg > 0);
	pids.push_back(database::package_id(pkg));
      }
      COMPARE_STRING(PQgetvalue(r1.raw, i, 1), "sysvinit-tools");
      COMPARE_STRING(PQgetvalue(r1.raw, i, 2), "2.88");
      std::string release(PQgetvalue(r1.raw, i, 3));
      const char *const *files;
      if (starts_with(release, "6")) {
	files = sysvinit_files_6;
      } else if (starts_with(release, "9")) {
	files = sysvinit_files_9;
      } else {
	CHECK(false);
	break;
      }
      const char *params[] = {PQgetvalue(r1.raw, i, 0)};
      pgresult_handle r2
	(PQexecParams(dbh.raw,
		      "SELECT name FROM symboldb.file"
		      " WHERE package = $1 ORDER BY 1",
		      1, NULL, params, NULL, NULL, 0));
      r2.check();
      int endj = PQntuples(r2.raw);
      CHECK(endj > 0);
      for (int j = 0; j < endj; ++j) {
	CHECK(files[j] != NULL);
	if (files[j] == NULL) {
	  break;
	}
	COMPARE_STRING(PQgetvalue(r2.raw, j, 0), files[j]);
      }
      CHECK(files[endj] == NULL);
    }
    r1.close();

    CHECK(!pids.empty());
    db.txn_begin();
    database::package_set_id pset(db.create_package_set("test-set", "x86_64"));
    CHECK(pset.value() > 0);
    CHECK(!db.update_package_set(pset, pids.begin(), pids.begin()));
    CHECK(db.update_package_set(pset, pids));
    CHECK(!db.update_package_set(pset, pids));
    db.txn_commit();

    r1.reset(PQexec(dbh.raw,
		    "SELECT * FROM symboldb.package_set_member"));
    r1.check();
    CHECK(PQntuples(r1.raw) == static_cast<int>(pids.size()));

    db.txn_begin();
    CHECK(db.update_package_set(pset, pids.begin() + 1, pids.end()));
    CHECK(!db.update_package_set(pset, pids.begin() + 1, pids.end()));
    db.txn_commit();
    r1.reset(PQexec(dbh.raw,
		    "SELECT * FROM symboldb.package_set_member"));
    r1.check();
    CHECK(PQntuples(r1.raw) == static_cast<int>(pids.size() - 1));
    char pkgstr[32];
    snprintf(pkgstr, sizeof(pkgstr), "%d", pids.front().value());
    {
      const char *params[] = {pkgstr};
      r1.reset(PQexecParams(dbh.raw,
			    "SELECT * FROM symboldb.package_set_member"
			    " WHERE package = $1",
		      1, NULL, params, NULL, NULL, 0));
      r1.check();
      CHECK(PQntuples(r1.raw) == 0);
    }
    db.txn_begin();
    CHECK(db.update_package_set(pset, pids.begin(), pids.begin() + 1));
    CHECK(!db.update_package_set(pset, pids.begin(), pids.begin() + 1));
    db.txn_commit();
    r1.reset(PQexec(dbh.raw,
		    "SELECT package FROM symboldb.package_set_member"));
    r1.check();
    CHECK(PQntuples(r1.raw) == 1);
    COMPARE_STRING(PQgetvalue(r1.raw, 0, 0), pkgstr);
  }

  // FIXME: Add more sanity check on database contents.
}

static test_register t("rpm_load", test);

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

#include "pg_testdb.hpp"
#include "pgconn_handle.hpp"
#include "pgresult_handle.hpp"
#include "pg_exception.hpp"
#include "pg_query.hpp"

#include "test.hpp"

static void
test()
{
  pg_testdb db;
  pgconn_handle h(db.connect("template1"));
  h.reset(db.connect("template1"));
  CHECK(db.notices().empty());
  db.exec_test_sql("template1", "CREATE TABLE abc(data TEXT)");
  db.exec_test_sql("template1", "INSERT INTO abc VALUES ('abc')");
  {
    pgresult_handle r;
    r.exec(h, "SELECT * FROM abc");
    CHECK(r.ntuples() == 1);
    COMPARE_STRING(r.getvalue(0, 0), "abc");
    r.exec(h, "CREATE TABLE test_table (pk TEXT PRIMARY KEY)");
    CHECK(db.notices().size() == 1);
    COMPARE_STRING(db.notices().at(0),
		   "NOTICE:  CREATE TABLE / PRIMARY KEY will create implicit"
		   " index \"test_table_pkey\" for table \"test_table\"\n");
  }
  {
    pgresult_handle r;
    try {
      r.exec(h, "garbage");
      CHECK(false);
    } catch (pg_exception &e) {
      COMPARE_STRING(e.severity_, "ERROR");
      COMPARE_STRING(e.primary_, "syntax error at or near \"garbage\"");
      COMPARE_STRING(e.sqlstate_, "42601");
      CHECK(e.statement_position_ == 1);
      CHECK(e.status_ == PGRES_FATAL_ERROR);
      COMPARE_STRING(PQresStatus(e.status_), "PGRES_FATAL_ERROR");
    }
  }

  {
    pgresult_handle r;
    {
      // All type dispatchers.
      short t1 = 0x0102;
      int t2 = 0x03040506;
      long long t3 = 0xa1a2a3a4a5a6a7a8LL;
      const char *t4 = "const char *";
      std::string t5 = "std::string";
      bool t6 = false;

      pg_query(h, r, "SELECT $1, $2, $3, $4, $5, $6", t1, t2, t3, t4, t5, t6);
      CHECK(r.ntuples() == 1);
      COMPARE_STRING(r.getvalue(0, 0), "258");
      COMPARE_STRING(r.getvalue(0, 1), "50595078");
      COMPARE_STRING(r.getvalue(0, 2), "-6799692559826901080");
      COMPARE_STRING(r.getvalue(0, 3), "const char *");
      COMPARE_STRING(r.getvalue(0, 4), "std::string");
      COMPARE_STRING(r.getvalue(0, 5), "f");

      r.exec(h, "SELECT 1");
      pg_query(h, r, "SELECT $1, $2, $3, $4", &t1, &t2, &t3, &t6);
      CHECK(r.ntuples() == 1);
      COMPARE_STRING(r.getvalue(0, 0), "258");
      COMPARE_STRING(r.getvalue(0, 1), "50595078");
      COMPARE_STRING(r.getvalue(0, 2), "-6799692559826901080");
      COMPARE_STRING(r.getvalue(0, 3), "f");

      t1 = 0xF1F2;
      t2 = 0xF3F4F5F6;
      t3 = 0x0102030405060708;
      t6 = true;
      pg_query(h, r, "SELECT $1, $2, $3, $4", t1, t2, t3, t6);
      CHECK(r.ntuples() == 1);
      COMPARE_STRING(r.getvalue(0, 0), "-3598");
      COMPARE_STRING(r.getvalue(0, 1), "-202050058");
      COMPARE_STRING(r.getvalue(0, 2), "72623859790382856");
      COMPARE_STRING(r.getvalue(0, 3), "t");

      t1 = 0xF1F2;
      t2 = 0xF3F4F5F6;
      t3 = 0x0102030405060708;
      t6 = true;
      pg_query(h, r, "SELECT $1, $2, $3, $4", &t1, &t2, &t3, &t6);
      CHECK(r.ntuples() == 1);
      COMPARE_STRING(r.getvalue(0, 0), "-3598");
      COMPARE_STRING(r.getvalue(0, 1), "-202050058");
      COMPARE_STRING(r.getvalue(0, 2), "72623859790382856");
      COMPARE_STRING(r.getvalue(0, 3), "t");
    }

    // Parameter order.
    pg_query(h, r, "SELECT ARRAY[$1]", 1);
    CHECK(r.ntuples() == 1);
    COMPARE_STRING(r.getvalue(0, 0), "{1}");
    pg_query(h, r, "SELECT ARRAY[$1, $2]", 1, 2);
    CHECK(r.ntuples() == 1);
    COMPARE_STRING(r.getvalue(0, 0), "{1,2}");
    pg_query(h, r, "SELECT ARRAY[$1, $2, $3]", 1, 2, 3);
    CHECK(r.ntuples() == 1);
    COMPARE_STRING(r.getvalue(0, 0), "{1,2,3}");
    pg_query(h, r, "SELECT ARRAY[$1, $2, $3, $4]", 1, 2, 3, 4);
    CHECK(r.ntuples() == 1);
    COMPARE_STRING(r.getvalue(0, 0), "{1,2,3,4}");
    pg_query(h, r, "SELECT ARRAY[$1, $2, $3, $4, $5]", 1, 2, 3, 4, 5);
    CHECK(r.ntuples() == 1);
    COMPARE_STRING(r.getvalue(0, 0), "{1,2,3,4,5}");
    pg_query(h, r, "SELECT ARRAY[$1, $2, $3, $4, $5]", 1, 2, 3, 4, 5);
    CHECK(r.ntuples() == 1);
    COMPARE_STRING(r.getvalue(0, 0), "{1,2,3,4,5}");
    pg_query(h, r, "SELECT ARRAY[$1, $2, $3, $4, $5, $6]", 1, 2, 3, 4, 5, 6);
    CHECK(r.ntuples() == 1);
    COMPARE_STRING(r.getvalue(0, 0), "{1,2,3,4,5,6}");
    pg_query(h, r, "SELECT ARRAY[$1, $2, $3, $4, $5, $6, $7]",
	     1, 2, 3, 4, 5, 6, 7);
    CHECK(r.ntuples() == 1);
    COMPARE_STRING(r.getvalue(0, 0), "{1,2,3,4,5,6,7}");
    pg_query(h, r, "SELECT ARRAY[$1, $2, $3, $4, $5, $6, $7, $8]",
	     1, 2, 3, 4, 5, 6, 7, 8);
    CHECK(r.ntuples() == 1);
    COMPARE_STRING(r.getvalue(0, 0), "{1,2,3,4,5,6,7,8}");
    pg_query(h, r, "SELECT ARRAY[$1, $2, $3, $4, $5, $6, $7, $8, $9]",
	     1, 2, 3, 4, 5, 6, 7, 8, 9);
    CHECK(r.ntuples() == 1);
    COMPARE_STRING(r.getvalue(0, 0), "{1,2,3,4,5,6,7,8,9}");
  }
}

static test_register t("pg_testdb", test);

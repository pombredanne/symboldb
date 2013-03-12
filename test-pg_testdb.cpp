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
#include "pg_response.hpp"

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
      std::vector<unsigned char> t7;
      t7.push_back(0);
      t7.push_back(64);
      t7.push_back(127);
      t7.push_back(128);
      t7.push_back(255);

      pg_query(h, r, "SELECT $1, $2, $3, $4, $5, $6, $7",
	       t1, t2, t3, t4, t5, t6, t7);
      CHECK(r.ntuples() == 1);
      COMPARE_STRING(r.getvalue(0, 0), "258");
      COMPARE_STRING(r.getvalue(0, 1), "50595078");
      COMPARE_STRING(r.getvalue(0, 2), "-6799692559826901080");
      COMPARE_STRING(r.getvalue(0, 3), "const char *");
      COMPARE_STRING(r.getvalue(0, 4), "std::string");
      COMPARE_STRING(r.getvalue(0, 5), "f");
      COMPARE_STRING(r.getvalue(0, 6), "\\x00407f80ff");

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

    ////////////////////////////////////////////////////////////////////
    // Response decoding

    // bool
    {
      bool t = true;
      r.exec(h, "SELECT 0 = 1");
      pg_response(r, 0, t);
      CHECK(!t);
      r.exec(h, "SELECT 1 = 1");
      pg_response(r, 0, t);
      CHECK(t);

      r.execBinary(h, "SELECT 0 = 1");
      pg_response(r, 0, t);
      CHECK(!t);
      r.execBinary(h, "SELECT 1 = 1");
      pg_response(r, 0, t);
      CHECK(t);
    }

    // short
    {
      short t = 1;
      r.exec(h, "SELECT 0");
      pg_response(r, 0, t);
      CHECK(t == 0);
      r.exec(h, "SELECT 1");
      pg_response(r, 0, t);
      CHECK(t == 1);
      r.exec(h, "SELECT 258");
      pg_response(r, 0, t);
      CHECK(t == 258);
      r.exec(h, "SELECT -8001");
      pg_response(r, 0, t);
      CHECK(t == -8001);
      r.exec(h, "SELECT 32767");
      pg_response(r, 0, t);
      CHECK(t == 32767);
      r.exec(h, "SELECT -32768");
      pg_response(r, 0, t);
      CHECK(t == -32768);

      r.execBinary(h, "SELECT 0::int2");
      pg_response(r, 0, t);
      CHECK(t == 0);
      r.execBinary(h, "SELECT 1::int2");
      pg_response(r, 0, t);
      CHECK(t == 1);
      r.execBinary(h, "SELECT 258::int2");
      pg_response(r, 0, t);
      CHECK(t == 258);
      r.execBinary(h, "SELECT (-8001)::int2");
      pg_response(r, 0, t);
      CHECK(t == -8001);
      r.execBinary(h, "SELECT 32767::int2");
      pg_response(r, 0, t);
      CHECK(t == 32767);
      r.execBinary(h, "SELECT (-32768)::int2");
      pg_response(r, 0, t);
      CHECK(t == -32768);
    }

    // int
    {
      int t = 1;
      r.exec(h, "SELECT 0");
      pg_response(r, 0, t);
      CHECK(t == 0);
      r.exec(h, "SELECT 1");
      pg_response(r, 0, t);
      CHECK(t == 1);
      r.exec(h, "SELECT 258");
      pg_response(r, 0, t);
      CHECK(t == 258);
      r.exec(h, "SELECT 16909060");
      pg_response(r, 0, t);
      CHECK(t == 16909060);
      r.exec(h, "SELECT -16909060");
      pg_response(r, 0, t);
      CHECK(t == -16909060);
      r.exec(h, "SELECT -8001");
      pg_response(r, 0, t);
      CHECK(t == -8001);
      r.exec(h, "SELECT 32767");
      pg_response(r, 0, t);
      CHECK(t == 32767);
      r.exec(h, "SELECT -32768");
      pg_response(r, 0, t);
      CHECK(t == -32768);
      r.exec(h, "SELECT (2^31 - 1)");
      pg_response(r, 0, t);
      CHECK(t == (1U << 31) - 1);
      r.exec(h, "SELECT (- 2^31)");
      pg_response(r, 0, t);
      CHECK(t == static_cast<int>(-(1U << 31)));

      r.execBinary(h, "SELECT 0::int4");
      pg_response(r, 0, t);
      CHECK(t == 0);
      r.execBinary(h, "SELECT 1::int4");
      pg_response(r, 0, t);
      CHECK(t == 1);
      r.execBinary(h, "SELECT 258::int4");
      pg_response(r, 0, t);
      CHECK(t == 258);
      r.execBinary(h, "SELECT 16909060::int4");
      pg_response(r, 0, t);
      CHECK(t == 16909060);
      r.execBinary(h, "SELECT (-16909060)::int4");
      pg_response(r, 0, t);
      CHECK(t == -16909060);
      r.execBinary(h, "SELECT (-8001)::int4");
      pg_response(r, 0, t);
      CHECK(t == -8001);
      r.execBinary(h, "SELECT 32767::int4");
      pg_response(r, 0, t);
      CHECK(t == 32767);
      r.execBinary(h, "SELECT (-32768)::int4");
      pg_response(r, 0, t);
      CHECK(t == -32768);
      r.execBinary(h, "SELECT (2^31 - 1)::int4");
      pg_response(r, 0, t);
      CHECK(t == (1U << 31) - 1);
      r.execBinary(h, "SELECT (- 2^31)::int4");
      pg_response(r, 0, t);
      CHECK(t == static_cast<int>(-(1U << 31)));
    }

    // long long
    {
      long long t = 1;
      r.exec(h, "SELECT 0");
      pg_response(r, 0, t);
      CHECK(t == 0);
      r.exec(h, "SELECT 1");
      pg_response(r, 0, t);
      CHECK(t == 1);
      r.exec(h, "SELECT 258");
      pg_response(r, 0, t);
      CHECK(t == 258);
      r.exec(h, "SELECT 16909060");
      pg_response(r, 0, t);
      CHECK(t == 16909060);
      r.exec(h, "SELECT -16909060");
      pg_response(r, 0, t);
      CHECK(t == -16909060);
      r.exec(h, "SELECT -8001");
      pg_response(r, 0, t);
      CHECK(t == -8001);
      r.exec(h, "SELECT 32767");
      pg_response(r, 0, t);
      CHECK(t == 32767);
      r.exec(h, "SELECT -32768");
      pg_response(r, 0, t);
      CHECK(t == -32768);
      r.exec(h, "SELECT (2^31 - 1)");
      pg_response(r, 0, t);
      CHECK(t == (1U << 31) - 1);
      r.exec(h, "SELECT (- 2^31)");
      pg_response(r, 0, t);
      CHECK(t == -(1LL << 31));
      r.exec(h, "SELECT (2::numeric^63 - 1)::int8");
      pg_response(r, 0, t);
      CHECK(t == (1ULL << 63) - 1);
      r.exec(h, "SELECT (- 2::numeric^63)::int8");
      pg_response(r, 0, t);
      CHECK(t == static_cast<long long>(-(1ULL << 63)));

      r.execBinary(h, "SELECT 0::int8");
      pg_response(r, 0, t);
      CHECK(t == 0);
      r.execBinary(h, "SELECT 1::int8");
      pg_response(r, 0, t);
      CHECK(t == 1);
      r.execBinary(h, "SELECT 258::int8");
      pg_response(r, 0, t);
      CHECK(t == 258);
      r.execBinary(h, "SELECT 16909060::int8");
      pg_response(r, 0, t);
      CHECK(t == 16909060);
      r.execBinary(h, "SELECT (-16909060)::int8");
      pg_response(r, 0, t);
      CHECK(t == -16909060);
      r.execBinary(h, "SELECT (-8001)::int8");
      pg_response(r, 0, t);
      CHECK(t == -8001);
      r.execBinary(h, "SELECT 32767::int8");
      pg_response(r, 0, t);
      CHECK(t == 32767);
      r.execBinary(h, "SELECT (-32768)::int8");
      pg_response(r, 0, t);
      CHECK(t == -32768);
      r.execBinary(h, "SELECT (2^31 - 1)::int8");
      pg_response(r, 0, t);
      CHECK(t == (1U << 31) - 1);
      r.execBinary(h, "SELECT (- 2^31)::int8");
      pg_response(r, 0, t);
      CHECK(t == -(1LL << 31));
      r.execBinary(h, "SELECT (2::numeric^63 - 1)::int8");
      pg_response(r, 0, t);
      CHECK(t == (1ULL << 63) - 1);
      r.execBinary(h, "SELECT (- 2::numeric^63)::int8");
      pg_response(r, 0, t);
      CHECK(t == static_cast<long long>(-(1ULL << 63)));
    }

    // std::string
    {
      std::string t = "t";
      r.exec(h, "SELECT ''");
      pg_response(r, 0, t);
      COMPARE_STRING(t, "");
      t = "t";
      r.exec(h, "SELECT ''::bytea");
      pg_response(r, 0, t);
      COMPARE_STRING(t, "");
      t = "t";
      r.execBinary(h, "SELECT ''::text");
      pg_response(r, 0, t);
      COMPARE_STRING(t, "");
      t = "t";
      r.execBinary(h, "SELECT ''::bytea");
      pg_response(r, 0, t);
      COMPARE_STRING(t, "");

      r.exec(h, "SELECT 'a'");
      pg_response(r, 0, t);
      COMPARE_STRING(t, "a");
      t = "t";
      r.exec(h, "SELECT 'ab'::bytea");
      pg_response(r, 0, t);
      COMPARE_STRING(t, "ab");
      t = "t";
      r.execBinary(h, "SELECT 'abc'::text");
      pg_response(r, 0, t);
      COMPARE_STRING(t, "abc");
      t = "t";
      r.execBinary(h, "SELECT 'abcd'::bytea");
      pg_response(r, 0, t);
      COMPARE_STRING(t, "abcd");

      r.exec(h, "SELECT '\\x00407f80ff'");
      pg_response(r, 0, t);
      COMPARE_STRING(t, "\\x00407f80ff");
      t = "t";
      r.exec(h, "SELECT '\\x00407f80ff'::bytea");
      pg_response(r, 0, t);
      COMPARE_STRING(t, std::string("\x00\x40\x7f\x80\xff", 5));
      t = "t";
      r.execBinary(h, "SELECT '\\x00407f80ff'::text");
      pg_response(r, 0, t);
      COMPARE_STRING(t, "\\x00407f80ff");
      t = "t";
      r.execBinary(h, "SELECT '\\x00407f80ff'::bytea");
      pg_response(r, 0, t);
      COMPARE_STRING(t, std::string("\x00\x40\x7f\x80\xff", 5));
    }

    // std::vector<unsigned char>
    {
      char buf[1] = {'t'};
      std::vector<unsigned char> t;
      t.assign(buf, buf + 1);
      r.exec(h, "SELECT ''");
      pg_response(r, 0, t);
      COMPARE_STRING(std::string(t.begin(), t.end()), "");
      t.assign(buf, buf + 1);
      r.exec(h, "SELECT ''::bytea");
      pg_response(r, 0, t);
      COMPARE_STRING(std::string(t.begin(), t.end()), "");
      t.assign(buf, buf + 1);
      r.execBinary(h, "SELECT ''::text");
      pg_response(r, 0, t);
      COMPARE_STRING(std::string(t.begin(), t.end()), "");
      t.assign(buf, buf + 1);
      r.execBinary(h, "SELECT ''::bytea");
      pg_response(r, 0, t);
      COMPARE_STRING(std::string(t.begin(), t.end()), "");

      r.exec(h, "SELECT 'a'");
      pg_response(r, 0, t);
      COMPARE_STRING(std::string(t.begin(), t.end()), "a");
      t.assign(buf, buf + 1);
      r.exec(h, "SELECT 'ab'::bytea");
      pg_response(r, 0, t);
      COMPARE_STRING(std::string(t.begin(), t.end()), "ab");
      t.assign(buf, buf + 1);
      r.execBinary(h, "SELECT 'abc'::text");
      pg_response(r, 0, t);
      COMPARE_STRING(std::string(t.begin(), t.end()), "abc");
      t.assign(buf, buf + 1);
      r.execBinary(h, "SELECT 'abcd'::bytea");
      pg_response(r, 0, t);
      COMPARE_STRING(std::string(t.begin(), t.end()), "abcd");

      r.exec(h, "SELECT '\\x00407f80ff'");
      pg_response(r, 0, t);
      COMPARE_STRING(std::string(t.begin(), t.end()), "\\x00407f80ff");
      t.assign(buf, buf + 1);
      r.exec(h, "SELECT '\\x00407f80ff'::bytea");
      pg_response(r, 0, t);
      COMPARE_STRING(std::string(t.begin(), t.end()),
		     std::string("\x00\x40\x7f\x80\xff", 5));
      t.assign(buf, buf + 1);
      r.execBinary(h, "SELECT '\\x00407f80ff'::text");
      pg_response(r, 0, t);
      COMPARE_STRING(std::string(t.begin(), t.end()), "\\x00407f80ff");
      t.assign(buf, buf + 1);
      r.execBinary(h, "SELECT '\\x00407f80ff'::bytea");
      pg_response(r, 0, t);
      COMPARE_STRING(std::string(t.begin(), t.end()),
		     std::string("\x00\x40\x7f\x80\xff", 5));
    }

    // Result order.
    {
      int t1, t2, t3, t4, t5, t6, t7, t8, t9;

      r.exec(h, "SELECT 1, 2");
      t1 = t2 = t3 = t4 = t5 = t6 = t7 = t8 = t9 = 0;
      pg_response(r, 0, t1, t2);
      CHECK(t1 == 1);
      CHECK(t2 == 2);

      r.exec(h, "SELECT 1, 2, 3");
      t1 = t2 = t3 = t4 = t5 = t6 = t7 = t8 = t9 = 0;
      pg_response(r, 0, t1, t2, t3);
      CHECK(t1 == 1);
      CHECK(t2 == 2);
      CHECK(t3 == 3);

      r.exec(h, "SELECT 1, 2, 3, 4");
      t1 = t2 = t3 = t4 = t5 = t6 = t7 = t8 = t9 = 0;
      pg_response(r, 0, t1, t2, t3, t4);
      CHECK(t1 == 1);
      CHECK(t2 == 2);
      CHECK(t3 == 3);
      CHECK(t4 == 4);

      r.exec(h, "SELECT 1, 2, 3, 4, 5");
      t1 = t2 = t3 = t4 = t5 = t6 = t7 = t8 = t9 = 0;
      pg_response(r, 0, t1, t2, t3, t4, t5);
      CHECK(t1 == 1);
      CHECK(t2 == 2);
      CHECK(t3 == 3);
      CHECK(t4 == 4);
      CHECK(t5 == 5);

      r.exec(h, "SELECT 1, 2, 3, 4, 5, 6");
      t1 = t2 = t3 = t4 = t5 = t6 = t7 = t8 = t9 = 0;
      pg_response(r, 0, t1, t2, t3, t4, t5, t6);
      CHECK(t1 == 1);
      CHECK(t2 == 2);
      CHECK(t3 == 3);
      CHECK(t4 == 4);
      CHECK(t5 == 5);
      CHECK(t6 == 6);

      r.exec(h, "SELECT 1, 2, 3, 4, 5, 6, 7");
      t1 = t2 = t3 = t4 = t5 = t6 = t7 = t8 = t9 = 0;
      pg_response(r, 0, t1, t2, t3, t4, t5, t6, t7);
      CHECK(t1 == 1);
      CHECK(t2 == 2);
      CHECK(t3 == 3);
      CHECK(t4 == 4);
      CHECK(t5 == 5);
      CHECK(t6 == 6);
      CHECK(t7 == 7);

      r.exec(h, "SELECT 1, 2, 3, 4, 5, 6, 7, 8");
      t1 = t2 = t3 = t4 = t5 = t6 = t7 = t8 = t9 = 0;
      pg_response(r, 0, t1, t2, t3, t4, t5, t6, t7, t8);
      CHECK(t1 == 1);
      CHECK(t2 == 2);
      CHECK(t3 == 3);
      CHECK(t4 == 4);
      CHECK(t5 == 5);
      CHECK(t6 == 6);
      CHECK(t7 == 7);
      CHECK(t8 == 8);

      r.exec(h, "SELECT 1, 2, 3, 4, 5, 6, 7, 8, 9");
      t1 = t2 = t3 = t4 = t5 = t6 = t7 = t8 = t9 = 0;
      pg_response(r, 0, t1, t2, t3, t4, t5, t6, t7, t8, t9);
      CHECK(t1 == 1);
      CHECK(t2 == 2);
      CHECK(t3 == 3);
      CHECK(t4 == 4);
      CHECK(t5 == 5);
      CHECK(t6 == 6);
      CHECK(t7 == 7);
      CHECK(t8 == 8);
      CHECK(t9 == 9);
    }
  }
}

static test_register t("pg_testdb", test);

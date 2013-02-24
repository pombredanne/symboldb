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

#include "database.hpp"
#include "download.hpp"

#include "fd_handle.hpp"
#include "fd_source.hpp"
#include "os.hpp"
#include "pg_testdb.hpp"
#include "pgconn_handle.hpp"
#include "pgresult_handle.hpp"
#include "source_sink.hpp"
#include "vector_sink.hpp"

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

  static const char FILE[] = "test/data/primary.xml";
  std::vector<unsigned char> reference;
  {
    fd_handle h;
    h.open_read_only(FILE);
    fd_source src(h.raw);
    vector_sink vsink;
    copy_source_to_sink(src, vsink);
    reference.swap(vsink.data);
  }

  database db(testdb.directory().c_str(), DBNAME);
  download_options opt;
  opt.cache_mode = download_options::only_cache;
  std::string url("file://");
  url += current_directory();
  url += '/';
  url += FILE;
  std::vector<unsigned char> result;
  std::string error;
  CHECK(!download(opt, db, url.c_str(), result, error));
  CHECK(result.empty());
  COMPARE_STRING(error, "URL not in cache and network access disabled");

  opt = download_options();
  error.clear();
  CHECK(download(opt, db, url.c_str(), result, error));
  CHECK(result == reference);

  // FIXME: We should check somehow that this does not hit the
  // original file:/// URL.
  opt.cache_mode = download_options::only_cache;
  result.clear();
  error.clear();
  CHECK(download(opt, db, url.c_str(), result, error));
  CHECK(result == reference);

  // Make sure that we do not hit the database.
  testdb.exec_test_sql(DBNAME, "DROP TABLE symboldb.url_cache");

  opt.cache_mode = download_options::no_cache;
  result.clear();
  error.clear();
  CHECK(download(opt, db, url.c_str(), result, error));
  CHECK(result == reference);
}

static test_register t("download", test);

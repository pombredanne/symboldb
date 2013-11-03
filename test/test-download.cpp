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

#include <symboldb/database.hpp>
#include <symboldb/download.hpp>

#include <cxxll/fd_handle.hpp>
#include <cxxll/fd_source.hpp>
#include <cxxll/os.hpp>
#include <cxxll/pg_testdb.hpp>
#include <cxxll/pgconn_handle.hpp>
#include <cxxll/pgresult_handle.hpp>
#include <cxxll/source_sink.hpp>
#include <cxxll/vector_sink.hpp>
#include <cxxll/curl_exception.hpp>

#include "test.hpp"

using namespace cxxll;

static void
test()
{
  static const char DBNAME[] = "template1";
  pg_testdb testdb;
  {
    // Run this directly, to suppress notices.
    pgconn_handle db(testdb.connect(DBNAME));
    pgresult_handle res;
    res.exec(db, database::SCHEMA_BASE);
    res.exec(db, database::SCHEMA_INDEX);
  }

  static const char FILE[] = "test/data/primary.xml";
  std::vector<unsigned char> reference;
  {
    fd_handle h;
    h.open_read_only(FILE);
    fd_source src(h.get());
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
  try {
    download(opt, db, url.c_str());
    CHECK(false);
  } catch (curl_exception &e) {
    COMPARE_STRING(e.message(), "URL not in cache and network access disabled");
    COMPARE_STRING(e.url(), url);
  }

  vector_sink vsink;
  opt = download_options();
  copy_source_to_sink(*download(opt, db, url.c_str()), vsink);
  CHECK(vsink.data == reference);

  // FIXME: We should check somehow that this does not hit the
  // original file:/// URL.
  opt.cache_mode = download_options::only_cache;
  vsink.data.clear();
  copy_source_to_sink(*download(opt, db, url.c_str()), vsink);
  CHECK(vsink.data == reference);

  // Make sure that we do not hit the database.
  testdb.exec_test_sql(DBNAME, "DROP TABLE symboldb.url_cache");

  opt.cache_mode = download_options::no_cache;
  vsink.data.clear();
  copy_source_to_sink(*download(opt, db, url.c_str()), vsink);
  CHECK(vsink.data == reference);
}

static test_register t("download", test);

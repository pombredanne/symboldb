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

#include "fd_source.hpp"
#include "os.hpp"
#include "os_exception.hpp"
#include "pg_exception.hpp"
#include "pgconn_handle.hpp"
#include "source_sink.hpp"
#include "string_sink.hpp"
#include "subprocess.hpp"

#include <unistd.h>

namespace {
  static const char INITDB[] = "initdb";
  static const char POSTMASTER[] = "postgres";

  std::string
  postgresql_prefix()
  {
    std::string candidate("/usr/bin/");
    if (is_executable((candidate + INITDB).c_str())
	&& is_executable((candidate + INITDB).c_str())) {
      return candidate;
    } else {
      throw pg_exception("could not locate PostgreSQL server binaries");
    }
  }
}

struct pg_testdb::impl {
  std::string program_prefix;
  std::string directory;
  std::string logfile;
  subprocess server;

  impl();
  ~impl();

  void initdb();
  void configure();
  void start();
  void wait_for_socket();
};

pg_testdb::impl::impl()
  : program_prefix(postgresql_prefix()),
    directory(make_temporary_directory("/tmp/pg_testdb-")),
    logfile(directory + "log")
{
  try {
    initdb();
    configure();
    start();
    wait_for_socket();
  } catch (...) {
    server.destroy_nothrow();
    try {
      remove_directory_tree(directory.c_str());
    } catch (...) {
      // FIXME: log
    }
    throw;
  }
}

pg_testdb::impl::~impl()
{
  server.destroy_nothrow();
  try {
    remove_directory_tree(directory.c_str());
  } catch(...) {
    // FIXME: log
  }
}

void
pg_testdb::impl::initdb()
{
  subprocess initdb;
  initdb.command((program_prefix + INITDB).c_str());
  initdb.redirect(subprocess::in, subprocess::null);
  initdb.redirect(subprocess::out, subprocess::pipe);
  initdb
    .arg("-A").arg("ident")
    .arg("-D").arg(directory.c_str())
    .arg("-E").arg("UTF8")
    .arg("--locale").arg("en_US.UTF-8");
  initdb.start();
  fd_source out_source(initdb.pipefd(subprocess::out));
  string_sink out_string;
  copy_source_to_sink(out_source, out_string);
  if (initdb.wait() != 0) {
    fprintf(stderr, "%s", out_string.data.c_str());
    throw pg_exception("initdb failed");
  }
}

void
pg_testdb::impl::configure()
{
  std::string confpath(directory);
  confpath += "/postgresql.conf";
  FILE *conf = fopen(confpath.c_str(), "a");
  if (conf == NULL) {
    throw os_exception().function(fopen).path(confpath.c_str());
  }
  try {
    fprintf(conf, "unix_socket_directories = '%s'\n", directory.c_str());
    fprintf(conf, "unix_socket_permissions = 0700\n");
    if (ferror(conf)) {
      throw os_exception().function(fprintf).fd(fileno(conf)).defaults();
    }
  } catch (...) {
    fclose(conf);
    throw;
  }
  fclose(conf);
}

void
pg_testdb::impl::start()
{
  // FIXME: redirect output to a file
  server.command((program_prefix + POSTMASTER).c_str())
    .arg("-D").arg(directory.c_str())
    .arg("-h").arg("") // no TCP socket
    .arg("-F"); // disable fsync()
  server.start();
}

void
pg_testdb::impl::wait_for_socket()
{
  std::string socket(directory);
  socket += "/.s.PGSQL.5432";
  for (unsigned i = 0; i < 150; ++i) {
    if (path_exists(socket.c_str())) {
      return;
    }
    usleep(100 * 1000);
  }
  throw pg_exception(("socket did not appear at: " + socket).c_str());
}

pg_testdb::pg_testdb()
  : impl_(new impl)
{
}

pg_testdb::~pg_testdb()
{
}

PGconn *
pg_testdb::connect(const char *dbname)
{
  static const char *const keys[] = {
    "host", "port", "dbname",
  };
  const char *values[] = {
    impl_->directory.c_str(), "5432", dbname,
  };
  pgconn_handle handle(PQconnectdbParams(keys, values, 0));
  if (handle.raw == NULL) {
    throw std::bad_alloc();
  }
  if (PQstatus(handle.raw) != CONNECTION_OK) {
    throw pg_exception(handle.raw);
  }
  return handle.release();
}

const std::string &
pg_testdb::directory()
{
  return impl_->directory;
}

const std::string &
pg_testdb::logfile()
{
  return impl_->logfile;
}

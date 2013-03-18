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

#include <cxxll/pg_testdb.hpp>

#include <cxxll/fd_source.hpp>
#include <cxxll/fd_sink.hpp>
#include <cxxll/os.hpp>
#include <cxxll/os_exception.hpp>
#include <cxxll/pg_exception.hpp>
#include <cxxll/pgconn_handle.hpp>
#include <cxxll/pgresult_handle.hpp>
#include <cxxll/source_sink.hpp>
#include <cxxll/string_sink.hpp>
#include <cxxll/subprocess.hpp>
#include <cxxll/dir_handle.hpp>
#include <cxxll/fd_handle.hpp>

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

using namespace cxxll;

namespace {
  static const char INITDB[] = "initdb";
  static const char POSTMASTER[] = "postgres";

  bool
  check_path(const std::string &path)
  {
    return is_executable((path + INITDB).c_str())
      && is_executable((path + INITDB).c_str());
  }

  std::string
  postgresql_prefix()
  {
    std::string candidate("/usr/bin/");
    if (check_path(candidate)) {
      return candidate;
    }
    static const char pgpath[] = "/usr/lib/postgresql";
    if (is_directory(pgpath)) {
      dir_handle pgdir(pgpath);
      candidate = pgpath;
      candidate += '/';
      std::string best_candidate;
      double best_ver = 0.0;
      while (dirent *d = pgdir.readdir()) {
	double ver = 0.0;
	if ((sscanf(d->d_name, "%lf", &ver) != 1 && ver <= 0)
	    || ver < best_ver) {
	  continue;
	}
	candidate.resize(sizeof(pgpath));
	candidate += d->d_name;
	candidate += "/bin/";
	if (check_path(candidate)) {
	  best_candidate = candidate;
	  best_ver = ver;
	}
      }
      candidate = best_candidate;
    }
    if (candidate.empty()) {
      throw pg_exception("could not locate PostgreSQL server binaries");
    }
    return candidate;
  }


  // Checks if the configuration file mentions
  // "unix_socket_directories".
  bool
  uses_unix_socket_directories(const char *confpath)
  {
    char *line = NULL;
    FILE *conf = fopen(confpath, "r");
    if (conf == NULL) {
      throw os_exception().function(fopen).path(confpath);
    }
    bool found = false;
    try {
      size_t linesize = 0;
      while (true) {
	errno = 0;
	if (getline(&line, &linesize, conf) < 0) {
	  if (errno == 0) {
	    break;
	  } else {
	    throw os_exception().function(getline).fd(fileno(conf)).defaults();
	  }
	}
	found = strstr(line, "unix_socket_directories") != NULL;
	if (found) {
	  break;
	}
      }
    } catch(...) {
      fclose(conf);
      free(line);
      throw;
    }
    fclose(conf);
    free(line);
    return found;
  }
}

struct pg_testdb::impl {
  std::string program_prefix;
  std::string directory;
  std::string logfile;
  std::vector<std::string> notices;
  subprocess server;

  impl();
  ~impl();

  void initdb();
  void configure();
  void start();
  void wait_for_socket();

  static void notice_processor(void *arg, const char *message);
};

pg_testdb::impl::impl()
  : program_prefix(postgresql_prefix()),
    directory(make_temporary_directory("/tmp/pg_testdb-")),
    logfile(directory + "/server.log")
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

  // Older PostgreSQL versions use "unix_socket_directory", but 9.2
  // and later use "unix_socket_directories" (and recognize only this
  // variant).
  bool sockdir_plural = uses_unix_socket_directories(confpath.c_str());

  FILE *conf = fopen(confpath.c_str(), "a");
  if (conf == NULL) {
    throw os_exception().function(fopen).path(confpath.c_str());
  }
  try {
    fprintf(conf, "unix_socket_director%s = '%s'\n",
	    sockdir_plural ? "ies" : "y", directory.c_str());
    fprintf(conf, "unix_socket_permissions = 0700\n");
    fprintf(conf, "log_directory = '.'\n");
    fprintf(conf, "log_filename = 'server.log'\n");
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
  fd_handle fd;
  fd.open(logfile.c_str(), O_WRONLY | O_APPEND | O_CREAT | O_TRUNC, 0600);
  server.command((program_prefix + POSTMASTER).c_str())
    .arg("-D").arg(directory.c_str())
    .arg("-h").arg("") // no TCP socket
    .arg("-F")  // disable fsync()
    .redirect_to(subprocess::out, fd.get())
    .redirect_to(subprocess::err, fd.get());
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

void
pg_testdb::impl::notice_processor(void *arg, const char *message)
{
  impl *impl_ = static_cast<impl *>(arg);
  try {
    impl_->notices.push_back(message);
  } catch (...) {
    // FIXME: log
  }
}

// Check that the server has come up.  The socket can be there, but
// the server still rejects incomming connections.
static void
wait_for_server(pg_testdb *db)
{
  for (unsigned i = 0; i < 150; ++i) {
    try {
      pgconn_handle handle(db->connect("template1"));
      break;
    } catch (pg_exception &e) {
      if (e.message_ == "FATAL:  the database system is starting up") {
	usleep(100 * 1000);
	continue;
      }
      throw;
    }
  }
}

pg_testdb::pg_testdb()
  : impl_(new impl)
{
  wait_for_server(this);
}

pg_testdb::~pg_testdb()
{
  if (std::uncaught_exception()) {
    try {
      dump_logs();
    } catch(...) {
      fprintf(stderr, "warning: exception while dupming PostgreSQL logs\n");
    }
  }
}

const std::vector<std::string> &
pg_testdb::notices() const
{
  return impl_->notices;
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

PGconn *
pg_testdb::connect(const char *dbname)
{
  static const char *keys[] = {
    "host", "port", "dbname", NULL
  };
  const char *values[] = {
    impl_->directory.c_str(), "5432", dbname, NULL
  };
  pgconn_handle handle(PQconnectdbParams(keys, values, 0));
  PQsetNoticeProcessor(handle.get(), impl::notice_processor, impl_.get());
  return handle.release();
}

void
pg_testdb::exec_test_sql(const char *dbname, const char *sql)
{
  pgconn_handle conn(connect(dbname));
  pgresult_handle res;
  res.exec(conn, sql);
}

void
pg_testdb::dump_logs()
{
  fprintf(stderr, "* PostgreSQL server log:\n");
  fflush(stderr);
  fd_handle fd;
  fd.open_read_only(impl_->logfile.c_str());
  fd_source source(fd.get());
  fd_sink sink(STDERR_FILENO);
  copy_source_to_sink(source, sink);
}

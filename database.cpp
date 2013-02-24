/*
 * Copyright (C) 2012, 2013 Red Hat, Inc.
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
#include "rpm_file_info.hpp"
#include "rpm_package_info.hpp"
#include "elf_image.hpp"
#include "elf_symbol_definition.hpp"
#include "elf_symbol_reference.hpp"
#include "symboldb_config.h"

#include <assert.h>
#include <stdlib.h>

#include <libpq-fe.h>

// FIXME: We need to add a transaction runner, so that we can retry
// transactions on deadlock or update conflict.

// Database table names

#define PACKAGE_TABLE "symboldb.package"
#define PACKAGE_DIGEST_TABLE "symboldb.package_digest"
#define FILE_TABLE "symboldb.file"
#define ELF_FILE_TABLE "symboldb.elf_file"
#define ELF_DEFINITION_TABLE "symboldb.elf_definition"
#define ELF_REFERENCE_TABLE "symboldb.elf_reference"
#define ELF_NEEDED_TABLE "symboldb.elf_needed"
#define ELF_RPATH_TABLE "symboldb.elf_rpath"
#define ELF_RUNPATH_TABLE "symboldb.elf_runpath"
#define ELF_ERROR_TABLE "symboldb.elf_error"
#define PACKAGE_SET_TABLE "symboldb.package_set"
#define PACKAGE_SET_MEMBER_TABLE "symboldb.package_set_member"
#define URL_CACHE_TABLE "symboldb.url_cache"

// Include the schema.sql file.
const char database::SCHEMA[] = {
#include "schema.sql.inc"
  , 0
};

struct database::impl {
  PGconn *conn;
  
  impl()
    : conn(NULL)
  {
  }

  ~impl()
  {
    PQfinish(conn);
  }

  void check();
};

void
database::impl::check()
{
  if (conn == NULL) {
    throw std::bad_alloc();
  }
  if (PQstatus(conn) != CONNECTION_OK) {
    throw database_exception(PQerrorMessage(conn));
  }
}

struct pgresult_wrapper {
  PGresult *raw;
  pgresult_wrapper()
    : raw(NULL)
  {
  }

  ~pgresult_wrapper()
  {
    PQclear(raw);
  }

  void check();
};

void
pgresult_wrapper::check()
{
  if (raw == NULL) {
    throw std::bad_alloc();
  }
  switch (PQresultStatus(raw)) {
  case PGRES_EMPTY_QUERY:
  case PGRES_COMMAND_OK:
  case PGRES_TUPLES_OK:
  case PGRES_COPY_OUT:
  case PGRES_COPY_IN:
  case PGRES_COPY_BOTH:
#ifdef HAVE_PG_SINGLE_TUPLE
  case PGRES_SINGLE_TUPLE:
#endif
    return;
  case PGRES_BAD_RESPONSE:
  case PGRES_NONFATAL_ERROR:
  case PGRES_FATAL_ERROR:
    break;
  }
  const char *err = PQresultErrorMessage(raw);
  if (err == NULL || err[0] == '\0') {
    throw database_exception("unknown database error");
  } else {
    throw database_exception(err);
  }
}

database::database()
  : impl_(new impl)
{
  impl_->conn = PQconnectdb("");
  impl_->check();
}

database::~database()
{
}

void
database::txn_begin()
{
  pgresult_wrapper res;
  res.raw = PQexec(impl_->conn, "BEGIN");
  res.check();
}

void
database::txn_commit()
{
  pgresult_wrapper res;
  res.raw = PQexec(impl_->conn, "COMMIT");
  res.check();
}

void
database::txn_rollback()
{
  pgresult_wrapper res;
  res.raw = PQexec(impl_->conn, "ROLLBACK");
  res.check();
}

database::advisory_lock_guard::~advisory_lock_guard()
{
}

struct database::advisory_lock_impl : database::advisory_lock_guard {
  std::tr1::shared_ptr<database::impl> impl_;
  int a;
  int b;
  virtual ~advisory_lock_impl();
};

database::advisory_lock_impl::~advisory_lock_impl()
{
  char astr[32];
  snprintf(astr, sizeof(astr), "%d", a);
  char bstr[32];
  snprintf(bstr, sizeof(bstr), "%d", b);
  const char *params[] = {astr, bstr};

  try {
    pgresult_wrapper res;
    res.raw = PQexecParams
      (impl_->conn,
       "SELECT pg_advisory_unlock($1, $2)",
       2, NULL, params, NULL, NULL, 0);
    res.check();
  } catch(...) {
    // TODO: Not much we can do here.  Logging would be useful.
  }
}

database::advisory_lock
database::lock(int a, int b)
{
  char astr[32];
  snprintf(astr, sizeof(astr), "%d", a);
  char bstr[32];
  snprintf(bstr, sizeof(bstr), "%d", b);
  const char *params[] = {astr, bstr};
  pgresult_wrapper res;

  if (PQtransactionStatus(impl_->conn) == PQTRANS_INTRANS) {
    res.raw = PQexecParams
      (impl_->conn,
       "SELECT pg_advisory_xact_lock($1, $2)",
       2, NULL, params, NULL, NULL, 0);
    res.check();
    // As this is a NOP, we do not have to guard against exceptions
    // from the object allocation.
    return advisory_lock(new advisory_lock_guard);
  } else {
    // Allocate beforehand to avoid exceptions after acquiring the
    // lock.
    std::tr1::shared_ptr<advisory_lock_impl> lock(new advisory_lock_impl);
    res.raw = PQexecParams
      (impl_->conn,
       "SELECT pg_advisory_lock($1, $2)",
       2, NULL, params, NULL, NULL, 0);
    res.check();
    lock->impl_ = impl_;
    lock->a = a;
    lock->b = b;
    return lock;
  }
}


static int
get_id(pgresult_wrapper &res)
{
  res.check();
  if (PQntuples(res.raw) > 0) {
    char *val = PQgetvalue(res.raw, 0, 0);
    int id = atoi(val);
    if (id <= 0) {
      throw database_exception("database returned invalid ID");
    }
    return id;
  }
  return 0;
}

static int
get_id_force(pgresult_wrapper &res)
{
  int id = get_id(res);
  if (id <= 0) {
    throw database_exception("unexpected empty result set");
  }
  return id;
}

bool
database::intern_package(const rpm_package_info &pkg,
			 package_id &pkg_id)
{
  // FIXME: This needs a transaction and locking.

  // Try to locate existing row.
  {
    pgresult_wrapper res;
    const char *params[] = {
      pkg.hash.c_str(),
    };
    res.raw = PQexecParams
      (impl_->conn,
       "SELECT id FROM " PACKAGE_TABLE " WHERE hash = decode($1, 'hex')",
       1, NULL, params, NULL, NULL, 0);
    int id = get_id(res);
    if (id > 0) {
      pkg_id = id;
      return false;
    }
  }

  // Insert new row.
  {
    char epochstr[32];
    if (pkg.epoch >= 0) {
      snprintf(epochstr, sizeof(epochstr), "%d", pkg.epoch);
    }
    const char *params[] = {
      pkg.name.c_str(),
      pkg.epoch >= 0 ? epochstr : NULL,
      pkg.version.c_str(),
      pkg.release.c_str(),
      pkg.arch.c_str(),
      pkg.hash.c_str(),
      pkg.source_rpm.c_str(),
    };
    pgresult_wrapper res;
    res.raw = PQexecParams
      (impl_->conn,
       "INSERT INTO " PACKAGE_TABLE
       " (name, epoch, version, release, arch, hash, source)"
       " VALUES ($1, $2, $3, $4, $5, decode($6, 'hex'), $7) RETURNING id",
       7, NULL, params, NULL, NULL, 0);
    pkg_id = get_id_force(res);
    return true;
  }
}

void
database::add_package_digest(package_id pkg,
			     const std::vector<unsigned char> &digest)
{
  // FIXME: This needs a transaction and locking.

  if (digest.size() < 16) {
    throw std::logic_error("invalid digest length");
  }

  char pkgstr[32];
  snprintf(pkgstr, sizeof(pkgstr), "%d", pkg);

  static const Oid paramTypes[] = {23 /* INT4 */, 17 /* BYTEA */};
  const int paramLengths[] = {0, static_cast<int>(digest.size())};
  const char *params[] = {
    pkgstr,
    reinterpret_cast<const char *>(digest.data()),
  };
  static const int paramFormats[] = {0, 1};

  // Try to locate existing row.
  {
    pgresult_wrapper res;
    res.raw = PQexecParams
      (impl_->conn,
       "SELECT 1 FROM " PACKAGE_DIGEST_TABLE
       " WHERE package = $1 AND digest = $2",
       2, paramTypes, params, paramLengths, paramFormats, 0);
    res.check();
    if (PQntuples(res.raw) > 0) {
      return;
    }
  }

  // Insert new row.
  pgresult_wrapper res;
  res.raw = PQexecParams
    (impl_->conn,
     "INSERT INTO " PACKAGE_DIGEST_TABLE " (package, digest)"
     " VALUES ($1, $2)",
     2, paramTypes, params, paramLengths, paramFormats, 0);
  res.check();
}

database::package_id
database::package_by_digest(const std::vector<unsigned char> &digest)
{
  if (digest.size() < 16) {
    throw std::logic_error("invalid digest length");
  }
  static const Oid paramTypes[] = {17 /* BYTEA */};
  const int paramLengths[] = {static_cast<int>(digest.size())};
  const char *params[] = {
    reinterpret_cast<const char *>(digest.data()),
  };
  static const int paramFormats[] = {1};
  pgresult_wrapper res;
  res.raw = PQexecParams
    (impl_->conn,
     "SELECT package FROM " PACKAGE_DIGEST_TABLE " WHERE digest = $1",
     1, paramTypes, params, paramLengths, paramFormats, 0);
  return get_id(res);
}

database::file_id
database::add_file(package_id pkg, const rpm_file_info &info)
{
  // FIXME: This needs a transaction.
  assert(PQtransactionStatus(impl_->conn) == PQTRANS_INTRANS);
  char pkgstr[32];
  snprintf(pkgstr, sizeof(pkgstr), "%d", pkg);
  char mtimestr[32];
  snprintf(mtimestr, sizeof(mtimestr), "%d", info.mtime);
  char modestr[32];
  snprintf(modestr, sizeof(modestr), "%d", info.mode);
  const char *params[] = {
    pkgstr,
    info.name.c_str(),
    info.user.c_str(),
    info.group.c_str(),
    mtimestr,
    modestr,
    info.normalized ? "true" : "false",
  };
  pgresult_wrapper res;
  res.raw = PQexecParams
    (impl_->conn,
     "INSERT INTO " FILE_TABLE
     " (package, name, user_name, group_name, mtime, mode, normalized)"
     " VALUES ($1, $2, $3, $4, $5, $6, $7) RETURNING id",
     7, NULL, params, NULL, NULL, 0);
  return get_id_force(res);
}

void
database::add_elf_image(file_id file, const elf_image &image,
			const char *fallback_arch, const char *soname)
{
  if (fallback_arch == NULL) {
    throw std::logic_error("fallback_arch");
  }
  assert(PQtransactionStatus(impl_->conn) == PQTRANS_INTRANS);

  char filestr[32];
  snprintf(filestr, sizeof(filestr), "%d", file);
  char ei_class[32];
  snprintf(ei_class, sizeof(ei_class), "%d", image.ei_class());
  char ei_data[32];
  snprintf(ei_data, sizeof(ei_data), "%d", image.ei_data());
  char e_machine[32];
  snprintf(e_machine, sizeof(e_machine), "%d", image.e_machine());

  const char *arch = image.arch();
  if (arch == NULL) {
    arch = fallback_arch;
  }
  const char *params[] = {
    filestr,
    ei_class,
    ei_data,
    e_machine,
    arch,
    soname,
  };
  pgresult_wrapper res;
  res.raw = PQexecParams
    (impl_->conn,
     "INSERT INTO " ELF_FILE_TABLE
     " (file, ei_class, ei_data, e_machine, arch, soname)"
     " VALUES ($1, $2, $3, $4, $5, $6)",
     6, NULL, params, NULL, NULL, 0);
  res.check();
}

void
database::add_elf_symbol_definition(file_id file,
				    const elf_symbol_definition &def)
{
  assert(PQtransactionStatus(impl_->conn) == PQTRANS_INTRANS);
  char filestr[32];
  snprintf(filestr, sizeof(filestr), "%d", file);
  const char *params[] = {
    filestr,
    def.symbol_name.c_str(),
    def.vda_name.empty() ? NULL : def.vda_name.c_str(),
    def.default_version ? "t" : "f",
    def.type_name.c_str(),
    def.binding_name.c_str(),
    def.visibility(),
  };
  pgresult_wrapper res;
  res.raw = PQexecParams
    (impl_->conn,
     "INSERT INTO " ELF_DEFINITION_TABLE
     " (file, name, version, primary_version, symbol_type, binding,"
     " visibility)"
     " VALUES ($1, $2, $3, $4, $5, $6, $7)",
     7, NULL, params, NULL, NULL, 0);
  res.check();
}

void
database::add_elf_symbol_reference(file_id file,
				   const elf_symbol_reference &ref)
{
  assert(PQtransactionStatus(impl_->conn) == PQTRANS_INTRANS);
  char filestr[32];
  snprintf(filestr, sizeof(filestr), "%d", file);
  const char *params[] = {
    filestr,
    ref.symbol_name.c_str(),
    ref.vna_name.empty() ? NULL : ref.vna_name.c_str(),
    ref.type_name.c_str(),
    ref.binding_name.c_str(),
    ref.visibility(),
  };
  pgresult_wrapper res;
  res.raw = PQexecParams
    (impl_->conn,
     "INSERT INTO " ELF_REFERENCE_TABLE
     " (file, name, version, symbol_type, binding, visibility)"
     " VALUES ($1, $2, $3, $4, $5, $6)",
     6, NULL, params, NULL, NULL, 0);
  res.check();
}

void
database::add_elf_needed(file_id file, const char *name)
{
  // FIXME: This needs a transaction.
  assert(PQtransactionStatus(impl_->conn) == PQTRANS_INTRANS);
  char filestr[32];
  snprintf(filestr, sizeof(filestr), "%d", file);
  const char *params[] = {filestr, name};
  pgresult_wrapper res;
  res.raw = PQexecParams
    (impl_->conn,
     "INSERT INTO " ELF_NEEDED_TABLE " (file, name) VALUES ($1, $2)",
     2, NULL, params, NULL, NULL, 0);
  res.check();
}

void
database::add_elf_rpath(file_id file, const char *name)
{
  // FIXME: This needs a transaction.
  assert(PQtransactionStatus(impl_->conn) == PQTRANS_INTRANS);
  char filestr[32];
  snprintf(filestr, sizeof(filestr), "%d", file);
  const char *params[] = {filestr, name};
  pgresult_wrapper res;
  res.raw = PQexecParams
    (impl_->conn,
     "INSERT INTO " ELF_RPATH_TABLE " (file, path) VALUES ($1, $2)",
     2, NULL, params, NULL, NULL, 0);
  res.check();
}

void
database::add_elf_runpath(file_id file, const char *name)
{
  // FIXME: This needs a transaction.
  assert(PQtransactionStatus(impl_->conn) == PQTRANS_INTRANS);
  char filestr[32];
  snprintf(filestr, sizeof(filestr), "%d", file);
  const char *params[] = {filestr, name};
  pgresult_wrapper res;
  res.raw = PQexecParams
    (impl_->conn,
     "INSERT INTO " ELF_RUNPATH_TABLE " (file, path) VALUES ($1, $2)",
     2, NULL, params, NULL, NULL, 0);
  res.check();
}

void
database::add_elf_error(file_id file, const char *message)
{
  // FIXME: This needs a transaction.
  assert(PQtransactionStatus(impl_->conn) == PQTRANS_INTRANS);
  char filestr[32];
  snprintf(filestr, sizeof(filestr), "%d", file);
  const char *params[] = {filestr, message};
  pgresult_wrapper res;
  res.raw = PQexecParams
    (impl_->conn,
     "INSERT INTO " ELF_ERROR_TABLE " (file, message) VALUES ($1, $2)",
     2, NULL, params, NULL, NULL, 0);
  res.check();
}

database::package_set_id
database::create_package_set(const char *name, const char *arch)
{
  const char *params[] = {name, arch};
  pgresult_wrapper res;
  res.raw = PQexecParams
    (impl_->conn,
     "INSERT INTO " PACKAGE_SET_TABLE
     " (name, arch) VALUES ($1, $2) RETURNING id",
     2, NULL, params, NULL, NULL, 0);
  return get_id_force(res);
}

database::package_set_id
database::lookup_package_set(const char *name)
{
  const char *params[] = {name};
  pgresult_wrapper res;
  res.raw = PQexecParams
    (impl_->conn,
     "SELECT id FROM " PACKAGE_SET_TABLE
     " WHERE name = $1",
     1, NULL, params, NULL, NULL, 0);
  return get_id(res);
}

void
database::add_package_set(package_set_id set, package_id pkg)
{
  char setstr[32];
  snprintf(setstr, sizeof(setstr), "%d", set);
  char pkgstr[32];
  snprintf(pkgstr, sizeof(pkgstr), "%d", pkg);
  const char *params[] = {setstr, pkgstr};
  pgresult_wrapper res;
  res.raw = PQexecParams
    (impl_->conn,
     "INSERT INTO " PACKAGE_SET_MEMBER_TABLE
     " (set, package) VALUES ($1, $2)",
     2, NULL, params, NULL, NULL, 0);
  res.check();
}

void
database::empty_package_set(package_set_id set)
{
  char setstr[32];
  snprintf(setstr, sizeof(setstr), "%d", set);
  const char *params[] = {setstr};
  pgresult_wrapper res;
  res.raw = PQexecParams
    (impl_->conn, "DELETE FROM " PACKAGE_SET_MEMBER_TABLE " WHERE set = $1",
     1, NULL, params, NULL, NULL, 0);
  res.check();
}

void
database::update_package_set_caches(package_set_id set)
{
  char setstr[32];
  snprintf(setstr, sizeof(setstr), "%d", set);
  const char *params[] = {setstr};
  pgresult_wrapper res;
  res.raw = PQexecParams
    (impl_->conn, "SELECT symboldb.elf_closure_update($1)",
     1, NULL, params, NULL, NULL, 0);
  res.check();
}

bool
database::url_cache_fetch(const char *url, size_t expected_length,
			  long long expected_time,
			  std::vector<unsigned char> &data)
{
  char lenstr[32];
  snprintf(lenstr, sizeof(lenstr), "%zu", expected_length);
  char timestr[32];
  snprintf(timestr, sizeof(timestr), "%lld", expected_time);
  const char *params[] = {url, lenstr, timestr};
  pgresult_wrapper res;
  res.raw = PQexecParams
    (impl_->conn, "SELECT data FROM " URL_CACHE_TABLE
     " WHERE url = $1 AND LENGTH(data) = $2 AND http_time = $3",
     3, NULL, params, NULL, NULL, 1);
  res.check();
  if (PQntuples(res.raw) != 1) {
    return false;
  }
  data.clear();
  const char *ptr = PQgetvalue(res.raw, 0, 0);
  data.assign(ptr, ptr + PQgetlength(res.raw, 0, 0));
  return true;
}

bool
database::url_cache_fetch(const char *url,
			  std::vector<unsigned char> &data)
{
  const char *params[] = {url};
  pgresult_wrapper res;
  res.raw = PQexecParams
    (impl_->conn, "SELECT data FROM " URL_CACHE_TABLE " WHERE url = $1",
     1, NULL, params, NULL, NULL, 1);
  res.check();
  if (PQntuples(res.raw) != 1) {
    return false;
  }
  data.clear();
  const char *ptr = PQgetvalue(res.raw, 0, 0);
  data.assign(ptr, ptr + PQgetlength(res.raw, 0, 0));
  return true;
}

void
database::url_cache_update(const char *url,
			   const std::vector<unsigned char> &data,
			   long long time)
{
  char timestr[32];
  snprintf(timestr, sizeof(timestr), "%lld", time);
  const char *params[] = {
    url,
    timestr,
    reinterpret_cast<const char *>(data.data()),
  };
  static const Oid paramTypes[] = {25 /* TEXT */, 20 /* INT8 */, 17 /* BYTEA */};
  const int paramLengths[] = {0, 0, static_cast<int>(data.size())};
  if (static_cast<size_t>(paramLengths[2]) != data.size()) {
    throw std::runtime_error("data length out of range");
  }
  static const int paramFormats[] = {0, 0, 1};
  bool found;
  {
    pgresult_wrapper res;
    res.raw = PQexecParams
      (impl_->conn, "SELECT 1 FROM " URL_CACHE_TABLE " WHERE url = $1 FOR UPDATE",
       1, NULL, params, NULL, NULL, 0);
    res.check();
    found = PQntuples(res.raw) == 1;
  }
  pgresult_wrapper res;
  if (found) {
    res.raw = PQexecParams
      (impl_->conn, "UPDATE " URL_CACHE_TABLE
       " SET http_time = $2, data = $3, last_change = NOW() AT TIME ZONE 'UTC'"
       " WHERE url = $1",
       3, paramTypes, params, paramLengths, paramFormats, 0);
    res.check();
  } else {
    res.raw = PQexecParams
      (impl_->conn, "INSERT INTO " URL_CACHE_TABLE
       " (url, http_time, data, last_change)"
       " VALUES ($1, $2, $3, NOW() AT TIME ZONE 'UTC')",
       3, paramTypes, params, paramLengths, paramFormats, 0);
    res.check();
  }
}

void
database::print_elf_soname_conflicts(package_set_id set,
				     bool include_unreferenced)
{
  char setstr[32];
  snprintf(setstr, sizeof(setstr), "%d", set);
  const char *params[] = {setstr};
  pgresult_wrapper res;
  if (include_unreferenced) {
    res.raw = PQexecParams
      (impl_->conn,
       "SELECT c.arch, c.soname, f.name, symboldb.nevra(p)"
       " FROM (SELECT arch, soname, UNNEST(files) AS file"
       "  FROM symboldb.elf_soname_provider"
       "  WHERE package_set = $1 AND array_length(files, 1) > 1) c"
       " JOIN symboldb.file f ON c.file = f.id"
       " JOIN symboldb.package p ON f.package = p.id"
       " ORDER BY c.arch::text, soname, LENGTH(f.name), f.name",
       1, NULL, params, NULL, NULL, 0);
  } else {
    res.raw = PQexecParams
      (impl_->conn,
       "SELECT c.arch, c.soname, f.name, symboldb.nevra(p)"
       " FROM (SELECT arch, soname, UNNEST(files) AS file"
       "  FROM symboldb.elf_soname_provider"
       "  WHERE package_set = $1 AND array_length(files, 1) > 1"
       "  AND soname IN (SELECT en.name"
       "    FROM symboldb.package_set_member psm"
       "    JOIN symboldb.file f ON psm.package = f.package"
       "    JOIN symboldb.elf_file ef ON f.id = ef.file"
       "    JOIN symboldb.elf_needed en ON en.file = f.id"
       "    WHERE psm.set = $1)) c"
       " JOIN symboldb.file f ON c.file = f.id"
       " JOIN symboldb.package p ON f.package = p.id"
       " ORDER BY c.arch::text, soname, LENGTH(f.name), f.name",
       1, NULL, params, NULL, NULL, 0);
  }
  res.check();

  std::string arch;
  std::string soname;
  int rows = PQntuples(res.raw);
  if (rows == 0) {
    printf("No soname conflicts detected.");
    return;
  }
  bool first = true;
  for (int row = 0; row < rows; ++row) {
    const char *current_arch = PQgetvalue(res.raw, row, 0);
    const char *current_soname = PQgetvalue(res.raw, row, 1);
    const char *file = PQgetvalue(res.raw, row, 2);
    const char *pkg = PQgetvalue(res.raw, row, 3);
    bool primary = false;
    if (current_arch != arch) {
      if (first) {
	first = false;
      } else {
	printf("\n\n");
      }
      printf("* Architecture: %s\n", current_arch);
      arch = current_arch;
      primary = true;
    } else if (current_soname != soname) {
      primary = true;
      soname = current_soname;
    }
    if (primary) {
      printf("\nsoname: %s\n", current_soname);
    }
    printf("    %c %s (from %s)\n",
	   primary ? '*' : ' ', file, pkg);
  }
  printf("\nThe chosen DSO for each soname is marked with \"*\".\n");
  if (!include_unreferenced) {
    printf("Re-run with --verbose to see conflicts "
	   "involving unreferenced sonames.\n");
  }
}

void
database::create_schema()
{
  pgresult_wrapper res;
  res.raw = PQexec(impl_->conn, SCHEMA);
  res.check();
}

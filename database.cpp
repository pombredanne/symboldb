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
#include "database_elf_closure.hpp"
#include "rpm_file_info.hpp"
#include "rpm_package_info.hpp"
#include "elf_image.hpp"
#include "elf_symbol_definition.hpp"
#include "elf_symbol_reference.hpp"
#include "pgconn_handle.hpp"
#include "pgresult_handle.hpp"
#include "pg_exception.hpp"
#include "pg_query.hpp"
#include "pg_response.hpp"

#include <assert.h>
#include <stdlib.h>

#include <libpq-fe.h>

#include <set>

// FIXME: We need to add a transaction runner, so that we can retry
// transactions on deadlock or update conflict.

// Database table names

#define PACKAGE_TABLE "symboldb.package"
#define PACKAGE_DIGEST_TABLE "symboldb.package_digest"
#define FILE_TABLE "symboldb.file"
#define DIRECTORY_TABLE "symboldb.directory"
#define SYMLINK_TABLE "symboldb.symlink"
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
  pgconn_handle conn;
};

database::database()
  : impl_(new impl)
{
  impl_->conn.reset(PQconnectdb(""));
}

database::database(const char *host, const char *dbname)
  : impl_(new impl)
{
  static const char *keys[] = {
    "host", "port", "dbname", NULL
  };
  const char *values[] = {
    host, "5432", dbname, NULL
  };
  impl_->conn.reset(PQconnectdbParams(keys, values, 0));
}

database::~database()
{
}

void
database::txn_begin()
{
  pgresult_handle res;
  res.exec(impl_->conn, "BEGIN");
}

void
database::txn_commit()
{
  pgresult_handle res;
  res.exec(impl_->conn, "COMMIT");
}

void
database::txn_rollback()
{
  pgresult_handle res;
  res.exec(impl_->conn, "ROLLBACK");
}

void
database::txn_begin_no_sync()
{
  pgresult_handle res;
  res.exec(impl_->conn, "BEGIN; SET LOCAL synchronous_commit TO OFF");
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
  try {
    pgresult_handle res;
    pg_query(impl_->conn, res, "SELECT pg_advisory_unlock($1, $2)", a, b);
  } catch(...) {
    // TODO: Not much we can do here.  Logging would be useful.
  }
}

database::advisory_lock
database::lock(int a, int b)
{
  pgresult_handle res;
  if (impl_->conn.transactionStatus() == PQTRANS_INTRANS) {
    pg_query(impl_->conn, res, "SELECT pg_advisory_xact_lock($1, $2)", a, b);
    // As this is a NOP, we do not have to guard against exceptions
    // from the object allocation.
    return advisory_lock(new advisory_lock_guard);
  } else {
    // Allocate beforehand to avoid exceptions after acquiring the
    // lock.
    std::tr1::shared_ptr<advisory_lock_impl> lock(new advisory_lock_impl);
    pg_query(impl_->conn, res,  "SELECT pg_advisory_lock($1, $2)", a, b);
    lock->impl_ = impl_;
    lock->a = a;
    lock->b = b;
    return lock;
  }
}


static int
get_id(pgresult_handle &res)
{
  if (res.ntuples() > 0) {
    int id;
    pg_response(res, 0, id);
    if (id <= 0) {
      throw pg_exception("database returned invalid ID");
    }
    return id;
  }
  return 0;
}

static int
get_id_force(pgresult_handle &res)
{
  int id = get_id(res);
  if (id <= 0) {
    throw pg_exception("unexpected empty result set");
  }
  return id;
}

bool
database::intern_package(const rpm_package_info &pkg,
			 package_id &pkg_id)
{
  // FIXME: This needs a transaction and locking.

  // Try to locate existing row.
  pgresult_handle res;
  pg_query_binary
    (impl_->conn, res,
     "SELECT id FROM " PACKAGE_TABLE " WHERE hash = decode($1, 'hex')",
     pkg.hash);
  int id = get_id(res);
  if (id > 0) {
    pkg_id = package_id(id);
    return false;
  }

  // Insert new row.
  pg_query_binary
    (impl_->conn, res,
     "INSERT INTO " PACKAGE_TABLE
     " (name, epoch, version, release, arch, hash, source,"
     " build_host, build_time)"
     " VALUES ($1, $2, $3, $4, $5::symboldb.rpm_arch, decode($6, 'hex'), $7,"
     " $8, 'epoch'::TIMESTAMP WITHOUT TIME ZONE + '1 second'::interval * $9)"
     " RETURNING id",
     pkg.name, pkg.epoch >= 0 ? &pkg.epoch : NULL, pkg.version, pkg.release,
     pkg.arch, pkg.hash, pkg.source_rpm, pkg.build_host, pkg.build_time);
  pkg_id = package_id(get_id_force(res));
  return true;
}

void
database::add_package_digest(package_id pkg,
			     const std::vector<unsigned char> &digest,
			     unsigned long long length)
{
  // FIXME: This needs a transaction and locking.

  if (digest.size() < 16) {
    throw std::logic_error("invalid digest length");
  }
  if (length > (1ULL << 60)) {
    throw std::logic_error("invalid length");
  }

  // Try to locate existing row.
  pgresult_handle res;
  pg_query
    (impl_->conn, res,
     "SELECT 1 FROM " PACKAGE_DIGEST_TABLE
     " WHERE package = $1 AND digest = $2 AND length = $3",
     pkg.value(), digest, static_cast<long long>(length));
  if (res.ntuples() > 0) {
    return;
  }

  // Insert new row.
  pg_query
    (impl_->conn, res,
     "INSERT INTO " PACKAGE_DIGEST_TABLE " (package, digest, length)"
     " VALUES ($1, $2, $3)",
     pkg.value(), digest, static_cast<long long>(length));
}

database::package_id
database::package_by_digest(const std::vector<unsigned char> &digest)
{
  if (digest.size() < 16) {
    throw std::logic_error("invalid digest length");
  }
  pgresult_handle res;
  pg_query_binary
    (impl_->conn, res,
     "SELECT package FROM " PACKAGE_DIGEST_TABLE " WHERE digest = $1",
     digest);
  return package_id(get_id(res));
}

database::file_id
database::add_file(package_id pkg, const rpm_file_info &info,
		   std::vector<unsigned char> &digest,
		   std::vector<unsigned char> &contents)
{
  // FIXME: This needs a transaction.
  assert(impl_->conn.transactionStatus() == PQTRANS_INTRANS);
  long long length = info.digest.length;
  if (length < 0) {
    std::runtime_error("file length out of range");
  }
  pgresult_handle res;
  pg_query_binary
    (impl_->conn, res,
     "INSERT INTO " FILE_TABLE
     " (package, name, length, user_name, group_name, mtime, mode, normalized,"
     " digest, contents)"
     " VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10) RETURNING id",
     pkg.value(), info.name, length,  info.user, info.group,
     static_cast<long long>(info.mtime), static_cast<long long>(info.mode),
     info.normalized, digest, contents);
  return file_id(get_id_force(res));
}

void
database::add_directory(package_id pkg, const rpm_file_info &info)
{
  // FIXME: This needs a transaction.
  assert(impl_->conn.transactionStatus() == PQTRANS_INTRANS);
  pgresult_handle res;
  pg_query
    (impl_->conn, res,
     "INSERT INTO " DIRECTORY_TABLE
     " (package, name, user_name, group_name, mtime, mode, normalized)"
     " VALUES ($1, $2, $3, $4, $5, $6, $7)",
     pkg.value(), info.name, info.user, info.group,
     static_cast<long long>(info.mtime),
     static_cast<long long>(info.mode),
     info.normalized);
}

void
database::add_symlink(package_id pkg, const rpm_file_info &info,
		      const std::vector<unsigned char> &contents)
{
  // FIXME: This needs a transaction.
  assert(impl_->conn.transactionStatus() == PQTRANS_INTRANS);
  assert(info.is_symlink());
  std::string target(contents.begin(), contents.end());
  if (target.empty() || target.find('\0') != std::string::npos) {
    throw std::runtime_error("symlink with invalid target");
  }
  pgresult_handle res;
  pg_query
    (impl_->conn, res,
     "INSERT INTO " SYMLINK_TABLE
     " (package, name, target, user_name, group_name, mtime, normalized)"
     " VALUES ($1, $2, $3, $4, $5, $6, $7)",
     pkg.value(), info.name, target, info.user, info.group,
     static_cast<long long>(info.mtime),
     info.normalized);
}

void
database::add_elf_image(file_id file, const elf_image &image,
			const char *soname)
{
  assert(impl_->conn.transactionStatus() == PQTRANS_INTRANS);
  pgresult_handle res;
  pg_query
    (impl_->conn, res,
     "INSERT INTO " ELF_FILE_TABLE
     " (file, ei_class, ei_data, e_type, e_machine, arch, soname, build_id)"
     " VALUES ($1, $2, $3, $4, $5, $6::symboldb.elf_arch, $7, $8)",
     file.value(),
     static_cast<int>(image.ei_class()),
     static_cast<int>(image.ei_data()),
     static_cast<int>(image.e_type()),
     static_cast<int>(image.e_machine()),
     image.arch(),
     soname,
     image.build_id().empty() ? NULL : &image.build_id());
}

void
database::add_elf_symbol_definition(file_id file,
				    const elf_symbol_definition &def)
{
  assert(impl_->conn.transactionStatus() == PQTRANS_INTRANS);
  pgresult_handle res;
  int xsection = def.xsection;
  pg_query
    (impl_->conn, res,
     "INSERT INTO " ELF_DEFINITION_TABLE
     " (file, name, version, primary_version, symbol_type, binding,"
     " section, xsection, visibility)"
     " VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9::symboldb.elf_visibility)",
     file.value(),
     def.symbol_name.c_str(),
     def.vda_name.empty() ? NULL : def.vda_name.c_str(),
     def.default_version,
     static_cast<int>(def.type),
     static_cast<int>(def.binding),
     static_cast<short>(def.section),
     def.has_xsection() ? &xsection : NULL,
     def.visibility());
}

void
database::add_elf_symbol_reference(file_id file,
				   const elf_symbol_reference &ref)
{
  assert(impl_->conn.transactionStatus() == PQTRANS_INTRANS);
  pgresult_handle res;
  pg_query
    (impl_->conn, res,
     "INSERT INTO " ELF_REFERENCE_TABLE
     " (file, name, version, symbol_type, binding, visibility)"
     " VALUES ($1, $2, $3, $4, $5, $6::symboldb.elf_visibility)",
     file.value(),
     ref.symbol_name,
     ref.vna_name.empty() ? NULL : ref.vna_name.c_str(),
     static_cast<int>(ref.type),
     static_cast<int>(ref.binding),
     ref.visibility());
}

void
database::add_elf_needed(file_id file, const char *name)
{
  // FIXME: This needs a transaction.
  assert(impl_->conn.transactionStatus() == PQTRANS_INTRANS);
  pgresult_handle res;
  pg_query
    (impl_->conn, res,
     "INSERT INTO " ELF_NEEDED_TABLE " (file, name) VALUES ($1, $2)",
     file.value(), name);
}

void
database::add_elf_rpath(file_id file, const char *name)
{
  // FIXME: This needs a transaction.
  assert(impl_->conn.transactionStatus() == PQTRANS_INTRANS);
  pgresult_handle res;
  pg_query
    (impl_->conn, res,
     "INSERT INTO " ELF_RPATH_TABLE " (file, path) VALUES ($1, $2)",
     file.value(), name);
}

void
database::add_elf_runpath(file_id file, const char *name)
{
  // FIXME: This needs a transaction.
  assert(impl_->conn.transactionStatus() == PQTRANS_INTRANS);
  pgresult_handle res;
  pg_query
    (impl_->conn, res,
     "INSERT INTO " ELF_RUNPATH_TABLE " (file, path) VALUES ($1, $2)",
     file.value(), name);
}

void
database::add_elf_error(file_id file, const char *message)
{
  // FIXME: This needs a transaction.
  assert(impl_->conn.transactionStatus() == PQTRANS_INTRANS);
  pgresult_handle res;
  pg_query
    (impl_->conn, res,
     "INSERT INTO " ELF_ERROR_TABLE " (file, message) VALUES ($1, $2)",
     file.value(), message);
}

database::package_set_id
database::create_package_set(const char *name)
{
  pgresult_handle res;
  pg_query_binary
    (impl_->conn, res,
     "INSERT INTO " PACKAGE_SET_TABLE
     " (name) VALUES ($1) RETURNING id", name);
  return package_set_id(get_id_force(res));
}

database::package_set_id
database::lookup_package_set(const char *name)
{
  pgresult_handle res;
  pg_query_binary
    (impl_->conn, res,
     "SELECT id FROM " PACKAGE_SET_TABLE " WHERE name = $1", name);
  return package_set_id(get_id(res));
}

void
database::add_package_set(package_set_id set, package_id pkg)
{
  pgresult_handle res;
  pg_query
    (impl_->conn, res,
     "INSERT INTO " PACKAGE_SET_MEMBER_TABLE
     " (set, package) VALUES ($1, $2)", set.value(), pkg.value());
}

void
database::delete_from_package_set(package_set_id set, package_id pkg)
{
  pgresult_handle res;
  pg_query
    (impl_->conn, res,
     "DELETE FROM " PACKAGE_SET_MEMBER_TABLE " WHERE set = $1 AND package = $2",
     set.value(), pkg.value());
}

void
database::empty_package_set(package_set_id set)
{
  pgresult_handle res;
  pg_query
    (impl_->conn, res,
     "DELETE FROM " PACKAGE_SET_MEMBER_TABLE " WHERE set = $1", set.value());
}

bool
database::update_package_set(package_set_id set,
			     const std::vector<package_id> &pids)
{
  assert(impl_->conn.transactionStatus() == PQTRANS_INTRANS);
  bool changes = false;

  std::set<package_id> old;
  {
    pgresult_handle res;
    pg_query_binary
      (impl_->conn, res, "SELECT package FROM " PACKAGE_SET_MEMBER_TABLE
       " WHERE set = $1", set.value());

    for (int row = 0, end = res.ntuples(); row < end; ++row) {
      int pkg;
      pg_response(res, row, pkg);
      assert(pkg != 0);
      old.insert(package_id(pkg));
    }
  }

  for (std::vector<package_id>::const_iterator
	 p = pids.begin(), end = pids.end(); p != end; ++p) {
    package_id pkg = *p;
    if (old.erase(pkg) == 0) {
      // New package set member.
      add_package_set(set, pkg);
      changes = true;
    }
  }

  // Remaining old entries have to be deleted.
  for (std::set<package_id>::const_iterator
	 p = old.begin(), end = old.end(); p != end; ++p) {
    delete_from_package_set(set, *p);
    changes = true;
  }

  return changes;
}

void
database::update_package_set_caches(package_set_id set)
{
  update_elf_closure(impl_->conn, set);
}

bool
database::url_cache_fetch(const char *url, size_t expected_length,
			  long long expected_time,
			  std::vector<unsigned char> &data)
{
  if (expected_length > 1U << 30) {
    return false;
  }
  pgresult_handle res;
  pg_query_binary
    (impl_->conn, res, "SELECT data FROM " URL_CACHE_TABLE
     " WHERE url = $1 AND LENGTH(data) = $2 AND http_time = $3",
     url, static_cast<int>(expected_length), expected_time);
  if (res.ntuples() != 1) {
    return false;
  } else {
    pg_response(res, 0, data);
    return true;
  }
}

bool
database::url_cache_fetch(const char *url,
			  std::vector<unsigned char> &data)
{
  pgresult_handle res;
  pg_query_binary
    (impl_->conn, res,
     "SELECT data FROM " URL_CACHE_TABLE " WHERE url = $1", url);
  if (res.ntuples() != 1) {
    return false;
  }
  pg_response(res, 0, data);
  return true;
}

void
database::url_cache_update(const char *url,
			   const std::vector<unsigned char> &data,
			   long long time)
{
  pgresult_handle res;
  pg_query
    (impl_->conn, res,
     "SELECT 1 FROM " URL_CACHE_TABLE " WHERE url = $1 FOR UPDATE", url);

  if (res.ntuples() == 1) {
    pg_query
      (impl_->conn, res, "UPDATE " URL_CACHE_TABLE
       " SET http_time = $2, data = $3, last_change = NOW() AT TIME ZONE 'UTC'"
       " WHERE url = $1", url, time, data);
  } else {
    pg_query
      (impl_->conn, res, "INSERT INTO " URL_CACHE_TABLE
       " (url, http_time, data, last_change)"
       " VALUES ($1, $2, $3, NOW() AT TIME ZONE 'UTC')", url, time, data);
  }
}

void
database::referenced_package_digests
  (std::vector<std::vector<unsigned char> > &digests)
{
  pgresult_handle res;
  res.execBinary
    (impl_->conn,
     "SELECT digest FROM " PACKAGE_SET_MEMBER_TABLE " psm"
     " JOIN " PACKAGE_DIGEST_TABLE " d ON psm.package = d.package"
     " ORDER BY digest");
  std::vector<unsigned char> digest;
  for (int i = 0, end = res.ntuples(); i < end; ++i) {
    pg_response(res, i, digest);
    digests.push_back(digest);
  }
}

void
database::print_elf_soname_conflicts(package_set_id set,
				     bool include_unreferenced)
{
  pgresult_handle res;
  if (include_unreferenced) {
    pg_query
      (impl_->conn, res,
       "SELECT c.arch, c.soname, f.name, symboldb.nevra(p)"
       " FROM (SELECT arch, soname, UNNEST(files) AS file"
       "  FROM symboldb.elf_soname_provider"
       "  WHERE package_set = $1 AND array_length(files, 1) > 1) c"
       " JOIN symboldb.file f ON c.file = f.id"
       " JOIN symboldb.package p ON f.package = p.id"
       " ORDER BY c.arch::text, soname, LENGTH(f.name), f.name", set.value());
  } else {
    pg_query
      (impl_->conn, res,
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
       " ORDER BY c.arch::text, soname, LENGTH(f.name), f.name", set.value());
  }

  std::string arch;
  std::string soname;
  int rows = res.ntuples();
  if (rows == 0) {
    printf("No soname conflicts detected.");
    return;
  }
  bool first = true;
  for (int row = 0; row < rows; ++row) {
    std::string current_arch;
    std::string current_soname;
    std::string file;
    std::string pkg;
    pg_response(res, row, current_arch, current_soname, file, pkg);
    bool primary = false;
    if (current_arch != arch) {
      if (first) {
	first = false;
      } else {
	printf("\n\n");
      }
      printf("* Architecture: %s\n", current_arch.c_str());
      arch = current_arch;
      primary = true;
    } else if (current_soname != soname) {
      primary = true;
      soname = current_soname;
    }
    if (primary) {
      printf("\nsoname: %s\n", current_soname.c_str());
    }
    printf("    %c %s (from %s)\n",
	   primary ? '*' : ' ', file.c_str(), pkg.c_str());
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
  pgresult_handle res;
  res.exec(impl_->conn, SCHEMA);
}

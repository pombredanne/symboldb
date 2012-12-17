#include "database.hpp"
#include "rpm_file_info.hpp"
#include "rpm_package_info.hpp"
#include "elf_image.hpp"
#include "elf_symbol_definition.hpp"
#include "elf_symbol_reference.hpp"

#include <stdlib.h>

#include <libpq-fe.h>

// FIXME: We need to add a transaction runner, so that we can retry
// transactions on deadlock or update conflict.

// Database table names

#define PACKAGE_TABLE "symboldb.package"
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

// Include the schema.sql file.
static const char schema[] = {
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

database::file_id
database::add_file(package_id pkg, const rpm_file_info &info)
{
  // FIXME: This needs a transaction.
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
  };
  pgresult_wrapper res;
  res.raw = PQexecParams
    (impl_->conn,
     "INSERT INTO " FILE_TABLE
     " (package, name, user_name, group_name, mtime, mode)"
     " VALUES ($1, $2, $3, $4, $5, $6) RETURNING id",
     6, NULL, params, NULL, NULL, 0);
  return get_id_force(res);
}

void
database::add_elf_image(file_id file, const elf_image &image,
			const char *fallback_arch, const char *soname)
{
  if (fallback_arch == NULL) {
    throw std::logic_error("fallback_arch");
  }

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
  char filestr[32];
  snprintf(filestr, sizeof(filestr), "%d", file);
  const char *params[] = {
    filestr,
    def.symbol_name.c_str(),
    def.vda_name.empty() ? NULL : def.vda_name.c_str(),
    def.default_version ? "t" : "f",
  };
  pgresult_wrapper res;
  res.raw = PQexecParams
    (impl_->conn,
     "INSERT INTO " ELF_DEFINITION_TABLE
     " (file, name, version, primary_version) VALUES ($1, $2, $3, $4)",
     4, NULL, params, NULL, NULL, 0);
  res.check();
}

void
database::add_elf_symbol_reference(file_id file,
				   const elf_symbol_reference &ref)
{
  char filestr[32];
  snprintf(filestr, sizeof(filestr), "%d", file);
  const char *params[] = {
    filestr,
    ref.symbol_name.c_str(),
    ref.vna_name.empty() ? NULL : ref.vna_name.c_str(),
  };
  pgresult_wrapper res;
  res.raw = PQexecParams
    (impl_->conn,
     "INSERT INTO " ELF_REFERENCE_TABLE
     " (file, name, version) VALUES ($1, $2, $3)",
     3, NULL, params, NULL, NULL, 0);
  res.check();
}

void
database::add_elf_needed(file_id file, const char *name)
{
  // FIXME: This needs a transaction.
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
  res.raw = PQexec(impl_->conn, schema);
  res.check();
}

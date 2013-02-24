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

#pragma once

#include <stdexcept>
#include <string>
#include <tr1/memory>
#include <vector>

class rpm_file_info;
class rpm_package_info;
class elf_image;
class elf_symbol_definition;
class elf_symbol_reference;

// Members of the database class throw this exception on error.
struct database_exception : std::runtime_error {
  database_exception(const char *);
  database_exception(const std::string &);
};

class database {
  struct impl;
  std::tr1::shared_ptr<impl> impl_;
  struct advisory_lock_impl;

public:
  // Uses the environment to locate a database.
  database();
  ~database();

  // The database schema, as a sequence of PostgreSQL DDL statements.
  static const char SCHEMA[];
  
  void txn_begin();
  void txn_commit();
  void txn_rollback();

  struct advisory_lock_guard {
    virtual ~advisory_lock_guard();
  };
  typedef std::tr1::shared_ptr<advisory_lock_guard> advisory_lock;

  // Creates an advisory lock on this pair of integers.  The lock is a
  // transaction-scope lock if in a transaction.  Otherwise, it is a
  // plain advisory lock.  (The reason for this is that we cannot
  // unlock an regular advisory lock within an aborted transaction.)
  // Transaction-scoped locks are released when the transaction
  // concludes.
  advisory_lock lock(int, int);

  typedef int package_id;
  typedef int file_id;

  // Creates the "symboldb" database schema.
  void create_schema();

  // Returns true if the package_id was freshly added to the database.
  // FIXME: add source URI
  bool intern_package(const rpm_package_info &, package_id &);

  // Adds a digest of the file representation.  A single RPM with
  // identical contents can have multiple representations due to
  // different signatures and compression (and different digest).
  void add_package_digest(package_id, const std::vector<unsigned char> &digest);

  // Looks up a package ID by the external SHA-1 or SHA-256 digest.
  // Returns 0 if the package ID was not found.
  package_id package_by_digest(const std::vector<unsigned char> &digest);

  file_id add_file(package_id, const rpm_file_info &);

  // Loading ELF-related tables.

  // Populates the elf_file table.  Uses fallback_arch (from the RPM
  // header) in case we cannot determine the architecture from the ELF
  // header.
  void add_elf_image(file_id, const elf_image &, const char *fallback_arch,
		     const char *soname);

  void add_elf_symbol_definition(file_id, const elf_symbol_definition &);
  void add_elf_symbol_reference(file_id, const elf_symbol_reference &);
  void add_elf_needed(file_id, const char *);
  void add_elf_rpath(file_id, const char *);
  void add_elf_runpath(file_id, const char *);
  void add_elf_error(file_id, const char *);

  // Package sets.
  typedef int package_set_id;
  package_set_id create_package_set(const char *name, const char *arch);
  package_set_id lookup_package_set(const char *name);
  void add_package_set(package_set_id, package_id);

  // Remove all members from the package set.
  void empty_package_set(package_set_id);

  // Update packet-set-wide helper tables (such as ELF linkage).
  void update_package_set_caches(package_set_id);

  // Returns true if the URL has been cached with expected length and
  // modification time, and overwrites data.  Returns false otherwise.
  bool url_cache_fetch(const char *url, size_t expected_length,
		       long long expected_time,
		       std::vector<unsigned char> &data);

  // Returns true if the URL has been cached, and overwrites data.
  // Returns false otherwise.
  bool url_cache_fetch(const char *url, std::vector<unsigned char> &data);

  // Updates the cached data for this URL.
  void url_cache_update(const char *url,
			const std::vector<unsigned char> &data,
			long long time);

  // Debugging functions.
  void print_elf_soname_conflicts(package_set_id, bool include_unreferenced);
};

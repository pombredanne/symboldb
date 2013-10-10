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

#include <cxxll/tagged.hpp>

#include <stdexcept>
#include <string>
#include <tr1/memory>
#include <vector>

namespace cxxll {
  class rpm_dependency;
  class rpm_file_info;
  class rpm_package_info;
  class rpm_script;
  class rpm_trigger;
  class elf_image;
  class elf_symbol_definition;
  class elf_symbol_reference;
  class java_class;
  class maven_url;
}

// Database wrapper.
// Members of this class throw pg_exception on error.
class database {
  struct impl;
  std::tr1::shared_ptr<impl> impl_;
  struct advisory_lock_impl;

public:
  // Uses the environment to locate a database.
  database();
  ~database();

  // Uses a specific database name.
  database(const char *host, const char *dbname);

  // The database schema, as a sequence of PostgreSQL DDL statements.
  // The base schema lacks indexes.
  static const char SCHEMA_BASE[];
  static const char SCHEMA_INDEX[];
  
  void txn_begin();
  void txn_commit();
  void txn_rollback();

  // Transaction with synchronous_commit = off.
  void txn_begin_no_sync();

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

  // Lock the digest using its first 8 bytes.
  template <class RandomAccessIterator> advisory_lock
  lock_digest(RandomAccessIterator first, RandomAccessIterator last);

  // Lock namespace for package sets.
  enum { PACKAGE_SET_LOCK_TAG = 1667369644 };

  struct package_id_tag {};
  typedef cxxll::tagged<int, package_id_tag> package_id;
  struct contents_id_tag {};
  typedef cxxll::tagged<int, contents_id_tag> contents_id;
  struct attribute_id_tag {};
  typedef cxxll::tagged<int, attribute_id_tag> attribute_id;
  struct file_id_tag {};
  typedef cxxll::tagged<int, file_id_tag> file_id;

  // Creates the "symboldb" database schema.
  void create_schema(bool base, bool index);

  // Returns true if the package_id was freshly added to the database.
  // FIXME: add source URI
  bool intern_package(const cxxll::rpm_package_info &, package_id &);

  // Returns true if the contents_id was freshly added to the
  // database.  contents_id are specific to the file digest and inode
  // metadata (user name, group name, mode etc.), but not the
  // file name and the mtime.
  bool intern_file_contents(const cxxll::rpm_file_info &,
			    const std::vector<unsigned char> &digest,
			    const std::vector<unsigned char> &contents,
			    contents_id &);

  // Obtains an attribute ID for the attributes of this file.  This is
  // backed by the symbildb.file_attribute table.
  attribute_id intern_file_attribute(const cxxll::rpm_file_info &);

  // Adds a digest of the file representation.  A single RPM with
  // identical contents can have multiple representations due to
  // different signatures and compression (and different digest).
  // LENGTH is the size (on octets) of the RPM file.
  void add_package_digest(package_id, const std::vector<unsigned char> &digest,
			  unsigned long long length);

  // Adds the URL for an RPM package (that is, a location where it can
  // be downloaded).
  void add_package_url(package_id, const char *);

  // Looks up a package ID by the external SHA-1 or SHA-256 digest.
  // Returns 0 if the package ID was not found.
  package_id package_by_digest(const std::vector<unsigned char> &digest);

  // Adds a dependency for the package.
  void add_package_dependency(package_id, const cxxll::rpm_dependency &);

  // Adds a non-trigger script.
  void add_package_script(package_id, const cxxll::rpm_script &);

  // Adds a trigger script.  IDX is the ordinal of this trigger within
  // the RPM package.
  void add_package_trigger(package_id, const cxxll::rpm_trigger &, int idx);

  file_id add_file(package_id, const std::string &name, bool normalized,
		   long long mtime, int inode, contents_id, attribute_id);
  void add_file(package_id, const cxxll::rpm_file_info &,
		const std::vector<unsigned char> &digest,
		const std::vector<unsigned char> &contents,
		file_id &, contents_id &, bool &added, int &contents_length);

  // Replace the contents in the database with the supplied one.  This
  // can be called to fix up a truncated contents preview when we
  // encounter the same contents under a different name, and the name
  // indicates that we need to preserve the contents in full.
  void update_contents_preview(contents_id,
			       const std::vector<unsigned char> &);

  // Adds the directory to the database.
  void add_directory(package_id, const cxxll::rpm_file_info &);

  // Adds the symlink to the database.
  void add_symlink(package_id, const cxxll::rpm_file_info &);

  // Loading ELF-related tables.

  // Populates the elf_file table.  SONAME can be NULL.
  // Loads the elf_program_header table as well.
  void add_elf_image(contents_id, const cxxll::elf_image &, const char *soname);

  void add_elf_symbol_definition(contents_id,
				 const cxxll::elf_symbol_definition &);
  void add_elf_symbol_reference(contents_id,
				const cxxll::elf_symbol_reference &);
  void add_elf_needed(contents_id, const char *);
  void add_elf_rpath(contents_id, const char *);
  void add_elf_runpath(contents_id, const char *);
  void add_elf_dynamic(contents_id,
		       unsigned long long tag, unsigned long long value);
  void add_elf_error(contents_id, const char *);

  // XML extraction.
  void add_xml_error(contents_id, const char *message, unsigned line,
		     const std::vector<unsigned char> &before,
		     const std::vector<unsigned char> &after);

  // Java support.
  void add_java_class(contents_id, const cxxll::java_class &);
  void add_java_error(contents_id,
		      const char *message, const std::string &path);
  void add_maven_url(contents_id, const cxxll::maven_url &);

  // Python-related data.
  void add_python_import(contents_id, const char *name);
  void add_python_attribute(contents_id, const char *name);
  void add_python_function_def(contents_id, const char *name);
  void add_python_class_def(contents_id, const char *name);
  void add_python_error(contents_id, int line, const char *message);

  // Checks for already present Python analysis results, to prevent
  // duplicates.
  bool has_python_analysis(contents_id);

  // Package sets.
  struct package_set_tag {};
  typedef cxxll::tagged<int, package_set_tag> package_set_id;
  package_set_id create_package_set(const char *name);
  package_set_id lookup_package_set(const char *name);

  // Adds a single package to the package set.
  void add_package_set(package_set_id, package_id);

  // Removes a single package from the package set.
  void delete_from_package_set(package_set_id, package_id);

  // Remove all members from the package set.
  void empty_package_set(package_set_id);

  // Replaces the contents of the package set with the package IDs in
  // the vector.  Returns true if there were actual changes.
  bool update_package_set(package_set_id, const std::vector<package_id> &);
  template <class InputIterator> bool
  update_package_set(package_set_id, InputIterator first, InputIterator last);

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

  void referenced_package_digests(std::vector<std::vector<unsigned char> > &);

  // Expire unreferenced data.
  void expire_url_cache();
  void expire_packages();
  void expire_file_contents();
  void expire_java_classes();

  // Debugging functions.
  void print_elf_soname_conflicts(package_set_id);

  // Trap door into the database.
  void exec_sql(const char *command);

  // Obtains files with the specified digest.  Call next() until it
  // returns false, and call rpm_digest() and file_name() to obtain
  // the location of individual files.
  class files_with_digest {
    struct impl;
    std::tr1::shared_ptr<impl> impl_;
    files_with_digest(const files_with_digest &); // not implemented
    files_with_digest &operator=(const files_with_digest &); // not implemented
  public:
    files_with_digest(database &, const std::vector<unsigned char> &);
    ~files_with_digest();

    bool next();

    // The outer digest of the RPM package.  Can be used to locate the
    // file in the file cache.
    const std::vector<unsigned char> &rpm_digest() const;

    // The path within the RPM file.
    const std::string &file_name() const;
  };
};

template <class RandomAccessIterator> database::advisory_lock
database::lock_digest(RandomAccessIterator first, RandomAccessIterator last)
{
  if (last - first < 8) {
    throw std::logic_error("lock_digest: digest is too short");
  }
  int a = (*first & 0xFF) << 24;
  ++first;
  a |= (*first & 0xFF) << 16;
  ++first;
  a |= (*first & 0xFF) << 8;
  ++first;
  a |= *first & 0xFF;
  ++first;
  int b = (*first & 0xFF) << 24;
  ++first;
  b |= (*first & 0xFF) << 16;
  ++first;
  b |= (*first & 0xFF) << 8;
  ++first;
  b |= *first & 0xFF;
  return lock(a, b);
}

template <class InputIterator> bool
database::update_package_set(package_set_id set,
			     InputIterator first, InputIterator last)
{
  std::vector<package_id> pids;
  while (first != last) {
    pids.push_back(*first);
    ++first;
  }
  return update_package_set(set, pids);
}

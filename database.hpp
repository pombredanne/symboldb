#pragma once

#include <stdexcept>
#include <string>
#include <tr1/memory>

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

public:
  // Uses the environment to locate a database.
  database();
  ~database();
  
  void txn_begin();
  void txn_commit();

  typedef int package_id;
  typedef int file_id;

  // Returns true if the package_id was freshly added to the database.
  // FIXME: add source URI
  bool intern_package(const rpm_package_info &, package_id &);

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

  // Debugging functions.
  void print_elf_soname_conflicts(package_set_id, bool include_unreferenced);
};

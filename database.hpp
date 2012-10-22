#pragma once

#include <stdexcept>
#include <set>
#include <string>
#include <tr1/memory>

class rpm_file_info;
class rpm_package_info;
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
  void add_elf_symbol_definition(file_id, const elf_symbol_definition &);
  void add_elf_symbol_reference(file_id, const elf_symbol_reference &);
  void add_elf_needed(file_id, const std::set<std::string> &names);
  void add_elf_error(file_id, const char *);
};

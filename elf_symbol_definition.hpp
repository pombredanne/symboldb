#pragma once

#include "elf_symbol.hpp"

struct elf_symbol_definition : elf_symbol {
  std::string section_name;
  std::string vda_name;
  unsigned long long value;
  bool default_version;
  elf_symbol_definition();
  ~elf_symbol_definition();
};

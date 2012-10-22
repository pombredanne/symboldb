#pragma once

#include "elf_symbol.hpp"

struct elf_symbol_reference : elf_symbol {
  std::string vna_name;
  unsigned vna_other;
  elf_symbol_reference();
  ~elf_symbol_reference();
};

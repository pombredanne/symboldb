#pragma once

#include <string>

struct elf_symbol {
  std::string type_name;
  std::string binding_name;
  std::string visibility_type;
  std::string symbol_name;
  elf_symbol();
  ~elf_symbol();
};

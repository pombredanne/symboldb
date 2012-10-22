#pragma once

#include <libelf.h>

#include <tr1/functional>

class elf_symbol_definition;
class elf_symbol_reference;

struct find_symbols_callbacks {
  std::tr1::function<void(const elf_symbol_definition &)> definition;
  std::tr1::function<void(const elf_symbol_reference &)> reference;
};

void find_symbols(Elf *elf, const find_symbols_callbacks &);

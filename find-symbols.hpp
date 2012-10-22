#pragma once

#include <libelf.h>

#include <tr1/functional>
#include <stdexcept>

struct find_symbols_exception : std::runtime_error {
  find_symbols_exception(const char *);
  static void raise(const char *msg, ...)
    __attribute__((format(printf, 1, 2), noreturn));
};

class elf_symbol_definition;
class elf_symbol_reference;

struct find_symbols_callbacks {
  std::tr1::function<void(const elf_symbol_definition &)> definition;
  std::tr1::function<void(const elf_symbol_reference &)> reference;
};

void find_symbols(Elf *elf, const find_symbols_callbacks &);

#pragma once

#include <libelf.h>
#include <elfutils/libebl.h>

#include <tr1/functional>
#include <stdexcept>

struct find_symbols_exception : std::runtime_error {
  find_symbols_exception(const char *);
  static void raise(const char *msg, ...)
    __attribute__((format(printf, 1, 2), noreturn));
};

struct find_symbol_info {
  GElf_Sym *sym;
  const char *type_name;
  const char *binding_name;
  const char *visibility_type;
  const char *section_name;
  const char *symbol_name;
  const char *vna_name;
  const char *vda_name;
  unsigned vna_other;
};

void find_symbols(Elf *elf, std::tr1::function<void(const find_symbol_info &)>);

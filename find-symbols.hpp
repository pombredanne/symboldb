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

struct symbol_info {
  GElf_Sym *sym;
  const char *type_name;
  const char *binding_name;
  const char *visibility_type;
  const char *symbol_name;
};

struct defined_symbol_info : symbol_info {
  const char *section_name;
  const char *vda_name;
};

struct undefined_symbol_info : symbol_info {
  const char *vna_name;
  unsigned vna_other;
};

struct find_symbols_callbacks {
  std::tr1::function<void(const defined_symbol_info &)> defined;
  std::tr1::function<void(const undefined_symbol_info &)> undefined;
};


void find_symbols(Elf *elf, const find_symbols_callbacks &);

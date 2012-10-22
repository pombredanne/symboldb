#pragma once

#include <stdexcept>

struct elf_exception : std::runtime_error {
  elf_exception(const char *);
  static void raise(const char *msg, ...)
    __attribute__((format(printf, 1, 2), noreturn));
};

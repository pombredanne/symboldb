#include "find-symbols.hpp"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

find_symbols_exception::find_symbols_exception(const char *msg)
  : std::runtime_error(msg)
{
}

void
find_symbols_exception::raise(const char *msg, ...)
{
  va_list ap;
  char *buffer;
  va_start(ap, msg);
  int ret = vasprintf(&buffer, msg, ap);
  va_end(ap);
  if (ret < 0) {
    throw std::runtime_error("vasprintf failed");
  }
  try {
    throw find_symbols_exception(buffer);
  } catch (...) {
    free(buffer);
    throw;
  }
}

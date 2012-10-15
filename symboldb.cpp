

#include <fcntl.h>
#include <libelf.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include "find-symbols.hpp"

static void
dump_def(const defined_symbol_info &dsi)
{
  if (dsi.symbol_name == NULL || dsi.symbol_name[0] == '\0') {
    return;
  }
  printf("DEF %s %s\n", dsi.symbol_name, dsi.vda_name);
}

static void
dump_ref(const undefined_symbol_info &usi)
{
  if (usi.symbol_name == NULL || usi.symbol_name[0] == '\0') {
    return;
  }
  printf("REF %s %s\n", usi.symbol_name, usi.vna_name);
}

static find_symbols_callbacks fsc = {dump_def, dump_ref};

int
main(int argc, char **argv)
{
  elf_version(EV_CURRENT);

  int fd = ::open(argv[1], O_RDONLY);
  if (fd < 0) {
    fprintf(stderr, "open(%s): %m\n", argv[1]);
    exit(1);
  }
  Elf *e = elf_begin(fd, ELF_C_READ, NULL);
  if (e == NULL)  {
    fprintf(stderr, "elf_begin(%s): %s\n", argv[1], elf_errmsg(-1));
    exit(1);
  }
  find_symbols(e, fsc);
  elf_end(e);
  close(fd);

  return 0;
}

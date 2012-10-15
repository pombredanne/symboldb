

#include <fcntl.h>
#include <libelf.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include "find-symbols.hpp"

static void
dump(const find_symbol_info &fsi)
{
  printf("%s %s %s %s\n", fsi.symbol_name, fsi.section_name, fsi.vna_name, fsi.vda_name);
}

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
  find_symbols(e, dump);
  elf_end(e);
  close(fd);

  return 0;
}

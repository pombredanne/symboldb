

#include <fcntl.h>
#include <libelf.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include "find-symbols.hpp"
#include "rpm_parser.hpp"

static const char *elf_path; // FIXME

static void
dump_def(const defined_symbol_info &dsi)
{
  if (dsi.symbol_name == NULL || dsi.symbol_name[0] == '\0') {
    return;
  }
  printf("%s DEF %s %s 0x%llx%s\n", elf_path, dsi.symbol_name, dsi.vda_name,
	 (unsigned long long)dsi.sym->st_value,
	 dsi.default_version ? " [default]" : "");
}

static void
dump_ref(const undefined_symbol_info &usi)
{
  if (usi.symbol_name == NULL || usi.symbol_name[0] == '\0') {
    return;
  }
  printf("%s REF %s %s %llx\n", elf_path, usi.symbol_name, usi.vna_name,
	 (unsigned long long)usi.sym->st_value);
}

static find_symbols_callbacks fsc = {dump_def, dump_ref};

int
main(int argc, char **argv)
{
  if (argc != 2) {
    fprintf(stderr, "usage: %s RPM-FILE\n", argv[0]);
    exit(1);
  }

  elf_version(EV_CURRENT);
  rpm_parser_init();

  try {
    rpm_parser_state rpmst(argv[1]);
    cpio_entry ce;
    std::vector<char> name, contents;

    while (rpmst.read_file(ce, name, contents)) {
      fprintf(stderr, "*** [[%s]] %llu\n", &name.front(), (unsigned long long)contents.size());
      // Check if this is an ELF file.
      if (contents.size() > 4
	  && contents.at(0) == '\x7f'
	  && contents.at(1) == 'E'
	  && contents.at(2) == 'L'
	  && contents.at(3) == 'F') {

	Elf *e = elf_memory(&contents.front(), contents.size());
	if (e == NULL)  {
	  fprintf(stderr, "%s(%s): ELF error: %s\n",
		  argv[1], &name.front(), elf_errmsg(-1));
	  continue;
	}
	elf_path = &name.front(); // FIXME
	find_symbols(e, fsc);
	elf_end(e);
      }
    }
  } catch (rpm_parser_exception &e) {
    fprintf(stderr, "%s: RPM error: %s\n", argv[1], e.what());
    exit(1);
  }

  return 0;
}

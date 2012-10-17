

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
    rpm_file_entry file;

    while (rpmst.read_file(file)) {
      fprintf(stderr, "*** [[%s %s %s]] %llu\n", file.info->name.c_str(),
	      file.info->user.c_str(), file.info->group.c_str(),
	      (unsigned long long)file.contents.size());
      // Check if this is an ELF file.
      if (file.contents.size() > 4
	  && file.contents.at(0) == '\x7f'
	  && file.contents.at(1) == 'E'
	  && file.contents.at(2) == 'L'
	  && file.contents.at(3) == 'F') {

	Elf *e = elf_memory(&file.contents.front(), file.contents.size());
	if (e == NULL)  {
	  fprintf(stderr, "%s(%s): ELF error: %s\n",
		  argv[1], file.info->name.c_str(), elf_errmsg(-1));
	  continue;
	}
	elf_path = file.info->name.c_str(); // FIXME
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

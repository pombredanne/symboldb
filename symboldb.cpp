// For <inttypes.h>
#define __STDC_FORMAT_MACROS

#include <fcntl.h>
#include <getopt.h>
#include <inttypes.h>
#include <libelf.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include "find-symbols.hpp"
#include "rpm_parser.hpp"
#include "database.hpp"

namespace {
  struct options {
    enum {
      standard, verbose, quiet
    } output;

    options()
      : output(standard)
    {
    }
  };
}

static options opt;		// FIXME
static const char *elf_path; // FIXME
static database::file_id fid;
static std::tr1::shared_ptr<database> db; // FIXME

static void
dump_def(const defined_symbol_info &dsi)
{
  if (dsi.symbol_name == NULL || dsi.symbol_name[0] == '\0') {
    return;
  }
  if (opt.output == options::verbose) {
    fprintf(stderr, "%s DEF %s %s 0x%llx%s\n",
	    elf_path, dsi.symbol_name, dsi.vda_name,
	   (unsigned long long)dsi.sym->st_value,
	   dsi.default_version ? " [default]" : "");
  }
  db->add_elf_definition(fid, dsi);
}

static void
dump_ref(const undefined_symbol_info &usi)
{
  if (usi.symbol_name == NULL || usi.symbol_name[0] == '\0') {
    return;
  }
  if (opt.output == options::verbose) {
    fprintf(stderr, "%s REF %s %s %llx\n",
	    elf_path, usi.symbol_name, usi.vna_name,
	    (unsigned long long)usi.sym->st_value);
  }
  db->add_elf_reference(fid, usi);
}

static find_symbols_callbacks fsc = {dump_def, dump_ref};

static void
process_rpm(const char *rpm_path)
{
  try {
    rpm_parser_state rpmst(rpm_path);
    rpm_file_entry file;

    if (opt.output != options::quiet) {
      fprintf(stderr, "info: loading %s from %s\n", rpmst.nevra(), rpm_path);
    }
    database::package_id pkg = db->intern_package(rpmst.package());

    while (rpmst.read_file(file)) {
      if (opt.output == options::verbose) {
	fprintf(stderr, "%s %s %s %s %" PRIu32 " 0%o %llu\n",
		rpmst.nevra(), file.info->name.c_str(),
		file.info->user.c_str(), file.info->group.c_str(),
		file.info->mtime, file.info->mode,
		(unsigned long long)file.contents.size());
      }
      fid = db->add_file(pkg, *file.info); // FIXME
      // Check if this is an ELF file.
      if (file.contents.size() > 4
	  && file.contents.at(0) == '\x7f'
	  && file.contents.at(1) == 'E'
	  && file.contents.at(2) == 'L'
	  && file.contents.at(3) == 'F') {

	Elf *e = elf_memory(&file.contents.front(), file.contents.size());
	if (e == NULL)  {
	  fprintf(stderr, "%s(%s): ELF error: %s\n",
		  rpm_path, file.info->name.c_str(), elf_errmsg(-1));
	  continue;
	}
	elf_path = file.info->name.c_str(); // FIXME
	find_symbols(e, fsc);
	elf_end(e);
      }
    }
  } catch (rpm_parser_exception &e) {
    fprintf(stderr, "%s: RPM error: %s\n", rpm_path, e.what());
    exit(1);
  }
}

static void
usage(const char *progname)
{
  fprintf(stderr, "usage: %s RPM-FILE...\n", progname);
  exit(2);
}

int
main(int argc, char **argv)
{
  {
    int ch;
    while ((ch = getopt(argc, argv, "qv")) != -1) {
      switch (ch) {
      case 'q':
	opt.output = options::quiet;
	break;
      case 'v':
	opt.output = options::verbose;
	break;
      default:
	usage(argv[0]);
      }
    }
    if (optind >= argc) {
      usage(argv[0]);
    }
  }

  elf_version(EV_CURRENT);
  rpm_parser_init();
  db.reset(new database);

  db->txn_begin();
  for (int i = optind; i < argc; ++i) {
    process_rpm(argv[i]);
  }
  db->txn_commit();

  return 0;
}

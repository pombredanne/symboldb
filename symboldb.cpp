// For <inttypes.h>
#define __STDC_FORMAT_MACROS

#include <fcntl.h>
#include <getopt.h>
#include <inttypes.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include "elf_symbol_definition.hpp"
#include "elf_symbol_reference.hpp"
#include "elf_image.hpp"
#include "elf_exception.hpp"
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
dump_def(const elf_symbol_definition &def)
{
  if (def.symbol_name.empty()) {
    return;
  }
  if (opt.output == options::verbose) {
    fprintf(stderr, "%s DEF %s %s 0x%llx%s\n",
	    elf_path, def.symbol_name.c_str(), def.vda_name.c_str(),
	    def.value, def.default_version ? " [default]" : "");
  }
  db->add_elf_symbol_definition(fid, def);
}

static void
dump_ref(const elf_symbol_reference &ref)
{
  if (ref.symbol_name.empty()) {
    return;
  }
  if (opt.output == options::verbose) {
    fprintf(stderr, "%s REF %s %s\n",
	    elf_path, ref.symbol_name.c_str(), ref.vna_name.c_str());
  }
  db->add_elf_symbol_reference(fid, ref);
}

static void
process_rpm(const char *rpm_path)
{
  try {
    rpm_parser_state rpmst(rpm_path);
    rpm_file_entry file;

    database::package_id pkg;
    if (!db->intern_package(rpmst.package(), pkg)) {
      if (opt.output != options::quiet) {
	fprintf(stderr, "info: skipping %s from %s\n",
		rpmst.nevra(), rpm_path);
      }
      return;
    }

    if (opt.output != options::quiet) {
      fprintf(stderr, "info: loading %s from %s\n", rpmst.nevra(), rpm_path);
    }

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

	try {
	  elf_image image(&file.contents.front(), file.contents.size());
	  std::tr1::shared_ptr<elf_image::symbol_range> symbols =
	    image.symbols();
	  while (symbols->next()) {
	    if (symbols->definition()) {
	      dump_def(*symbols->definition());
	    } else if (symbols->reference()) {
	      dump_ref(*symbols->reference());
	    } else {
	      throw std::logic_error("unknown elf_symbol type");
	    }
	  }
	} catch (elf_exception e) {
	  fprintf(stderr, "%s(%s): ELF error: %s\n",
		  rpm_path, file.info->name.c_str(), e.what());
	  continue;
	}
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

  elf_image_init();
  rpm_parser_init();
  db.reset(new database);

  db->txn_begin();
  for (int i = optind; i < argc; ++i) {
    process_rpm(argv[i]);
  }
  db->txn_commit();

  return 0;
}

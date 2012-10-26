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
#include "rpm_parser_exception.hpp"
#include "database.hpp"

namespace {
  struct options {
    enum {
      standard, verbose, quiet
    } output;

    const char *arch;
    const char *set_name;

    options()
      : output(standard), arch(NULL), set_name(NULL)
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

static database::package_id
load_rpm(const char *rpm_path)
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
      return pkg;
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
	  {
	    elf_image::symbol_range symbols(image);
	    while (symbols.next()) {
	      if (symbols.definition()) {
		dump_def(*symbols.definition());
	      } else if (symbols.reference()) {
		dump_ref(*symbols.reference());
	      } else {
		throw std::logic_error("unknown elf_symbol type");
	      }
	    }
	  }
	  {
	    elf_image::dynamic_section_range dyn(image);
	    while (dyn.next()) {
	      switch (dyn.type()) {
	      case elf_image::dynamic_section_range::needed:
		db->add_elf_needed(fid, dyn.value().c_str());
		break;
	      case elf_image::dynamic_section_range::soname:
		db->add_elf_soname(fid, dyn.value().c_str());
		break;
	      case elf_image::dynamic_section_range::rpath:
		db->add_elf_rpath(fid, dyn.value().c_str());
		break;
	      case elf_image::dynamic_section_range::runpath:
		db->add_elf_runpath(fid, dyn.value().c_str());
		break;
	      }
	    }
	  }
	} catch (elf_exception e) {
	  fprintf(stderr, "%s(%s): ELF error: %s\n",
		  rpm_path, file.info->name.c_str(), e.what());
	  continue;
	}
      }
    }
    return pkg;
  } catch (rpm_parser_exception &e) {
    fprintf(stderr, "%s: RPM error: %s\n", rpm_path, e.what());
    exit(1);
  }
}

static int
do_load_rpm(const options &opt, char **argv)
{
  // Unreferenced RPMs should not be visible to analyzers, so we can
  // load each RPM in a separate transaction.
  for (; *argv; ++argv) {
    db->txn_begin();
    load_rpm(*argv);
    db->txn_commit();
  }
  return 0;
}

static int
do_create_set(const options &opt, char **argv)
{
  db->txn_begin();
  database::package_set_id set =
    db->create_package_set(opt.set_name, opt.arch);
  for (; *argv; ++argv) {
    database::package_id pkg = load_rpm(*argv);
    db->add_package_set(set, pkg);
  }
  db->txn_commit();
  return 0;
}

static void
usage(const char *progname, const char *error = NULL)
{
  if (error) {
    fprintf(stderr, "error: %s\n", error);
  }
  fprintf(stderr, "Usage:\n\n"
	  "  %1$s --load-rpm [OPTIONS] RPM-FILE...\n"
	  "  %1$s --create-set=NAME --arch=ARCH [OPTIONS] RPM-FILE...\n"
	  "\nOptions:\n"
	  "  --arch=ARCH, -a   base architecture\n"
	  "  --quiet, -q       less output\n"
	  "  --verbose, -v     more verbose output\n\n",
	  progname);
  exit(2);
}

namespace {
  namespace command {
    typedef enum {
      undefined = 1000,
      load_rpm,
      create_set,
    } type;
  };
}

int
main(int argc, char **argv)
{
  command::type cmd = command::undefined;
  {
    static const struct option long_options[] = {
      {"load-rpm", no_argument, 0, command::load_rpm},
      {"create-set", required_argument, 0, command::create_set},
      {"arch", required_argument, 0, 'a'},
      {"verbose", no_argument, 0, 'v'},
      {"quiet", no_argument, 0, 'q'},
      {0, 0, 0, 0}
    };
    int ch;
    int index;
    while ((ch = getopt_long(argc, argv, "a:qv", long_options, &index)) != -1) {
      switch (ch) {
      case 'a':
	opt.arch = optarg;
	break;
      case 'q':
	opt.output = options::quiet;
	break;
      case 'v':
	opt.output = options::verbose;
	break;
      case command::create_set:
	cmd = command::create_set;
	opt.set_name = optarg;
	break;
      case command::load_rpm:
	cmd = static_cast<command::type>(ch);
	break;
      default:
	usage(argv[0]);
      }
    }
    if (cmd == command::undefined) {
      usage(argv[0]);
    }
    if (opt.arch != NULL && opt.arch[0] == '\0') {
      usage(argv[0], "invalid architecture name");
    }
    if (opt.set_name != NULL && opt.set_name[0] == '\0') {
      usage(argv[0], "invalid package set name");
    }
    switch (cmd) {
    case command::load_rpm:
      if (argc == optind) {
	break;
      }
      break;
    case command::create_set:
      if (opt.arch == NULL) {
	usage(argv[0]);
      }
      break;
    case command::undefined:
      break;
    }
  }

  elf_image_init();
  rpm_parser_init();

  db.reset(new database);

  switch (cmd) {
  case command::load_rpm:
    do_load_rpm(opt, argv + optind);
    break;
  case command::create_set:
    return do_create_set(opt, argv + optind);
  case command::undefined:
  default:
    abort();
  }

  return 0;
}

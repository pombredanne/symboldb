/*
 * Copyright (C) 2012, 2013 Red Hat, Inc.
 * Written by Florian Weimer <fweimer@redhat.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

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
#include "rpm_package_info.hpp"
#include "rpm_parser_exception.hpp"
#include "database.hpp"
#include "package_set_consolidator.hpp"
#include "repomd.hpp"
#include "download.hpp"
#include "url.hpp"

#include <sstream>

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
load_rpm(const char *rpm_path, rpm_package_info &info)
{
  try {
    rpm_parser_state rpmst(rpm_path);
    info = rpmst.package();
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
	  std::string soname;
	  {
	    elf_image::dynamic_section_range dyn(image);
	    bool soname_seen = false;
	    while (dyn.next()) {
	      switch (dyn.type()) {
	      case elf_image::dynamic_section_range::needed:
		db->add_elf_needed(fid, dyn.value().c_str());
		break;
	      case elf_image::dynamic_section_range::soname:
		if (soname_seen) {
		  // The linker ignores some subsequent sonames, but
		  // not all of them.  Multiple sonames are rare.
		  if (dyn.value() != soname) {
		    std::ostringstream out;
		    out << "duplicate soname ignored: " << dyn.value()
			<< ", previous soname: " << soname;
		    db->add_elf_error(fid, out.str().c_str());
		  }
		} else {
		  soname = dyn.value();
		  soname_seen = true;
		}
		break;
	      case elf_image::dynamic_section_range::rpath:
		db->add_elf_rpath(fid, dyn.value().c_str());
		break;
	      case elf_image::dynamic_section_range::runpath:
		db->add_elf_runpath(fid, dyn.value().c_str());
		break;
	      }
	    }
	    if (!soname_seen) {
	      // The implicit soname is derived from the name of the
	      // binary.
	      size_t slashpos = file.info->name.rfind('/');
	      if (slashpos == std::string::npos) {
		soname = file.info->name;
	      } else {
		soname = file.info->name.substr(slashpos + 1);
	      }
	    }
	  }
	  db->add_elf_image(fid, image, info.arch.c_str(), soname.c_str());
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

static void
load_rpms(char **argv, package_set_consolidator &ids)
{
  // Unreferenced RPMs should not be visible to analyzers, so we can
  // load each RPM in a separate transaction.
  rpm_package_info info;
  for (; *argv; ++argv) {
    db->txn_begin();
    database::package_id pkg = load_rpm(*argv, info);
    ids.add(info, pkg);
    db->txn_commit();
  }
}

static int do_create_schema()
{
  db->create_schema();
  return 0;
}

static int
do_load_rpm(const options &opt, char **argv)
{
  package_set_consolidator ignored;
  load_rpms(argv, ignored);
  return 0;
}

static void
finalize_package_set(const options &opt, database::package_set_id set)
{
  if (opt.output != options::quiet) {
    fprintf(stderr, "info: updating package set caches\n");
  }
  db->update_package_set_caches(set);
}

static int
do_create_set(const options &opt, char **argv)
{
  if (db->lookup_package_set(opt.set_name) != 0){
    fprintf(stderr, "error: package set \"%s\" already exists\n",
	    opt.set_name);
    return 1;
  }

  typedef std::vector<database::package_id> pset;
  pset ids;
  {
    package_set_consolidator psc;
    load_rpms(argv, psc);
    ids = psc.package_ids();
  }

  db->txn_begin();
  database::package_set_id set =
    db->create_package_set(opt.set_name, opt.arch);
  for (pset::const_iterator p = ids.begin(), end = ids.end(); p != end; ++p) {
    db->add_package_set(set, *p);
  }
  finalize_package_set(opt, set);
  db->txn_commit();
  return 0;
}

static int
do_update_set(const options &opt, char **argv)
{
  database::package_set_id set = db->lookup_package_set(opt.set_name);
  if (set == 0) {
    fprintf(stderr, "error: package set \"%s\" does not exist\n",
	    opt.set_name);
    return 1;
  }

  typedef std::vector<database::package_id> pset;
  pset ids;
  {
    package_set_consolidator psc;
    load_rpms(argv, psc);
    ids = psc.package_ids();
  }

  db->txn_begin();
  db->empty_package_set(set);
  for (pset::const_iterator p = ids.begin(), end = ids.end(); p != end; ++p) {
    db->add_package_set(set, *p);
  }
  finalize_package_set(opt, set);
  db->txn_commit();
  return 0;
}

static int
do_download(const options &, database &db, const char *url)
{
  std::vector<unsigned char> data;
  std::string error;
  if (!download(db, url, data, error)) {
    fprintf(stderr, "error: %s: %s\n", url, error.c_str());
    return 1;
  }
  if (!data.empty()
      && fwrite(&data.front(), data.size(), 1, stdout) != 1) {
    perror("fwrite");
    return 1;
  }
  return 0;
}

static std::string
base_url_canon(const char *base)
{
  std::string base_canon(base);
  if (!base_canon.empty() && base_canon.at(base_canon.size() - 1) != '/') {
    base_canon += '/';
  }
  return base_canon;
}

static bool
acquire_repomd(database &db, const char *base, repomd &rp)
{
  std::string url(url_combine(base, "repodata/repomd.xml"));
  std::vector<unsigned char> data;
  std::string error;
  if (!download(db, url.c_str(), data, error)
      || !rp.parse(&data.front(), data.size(), error)) {
    fprintf(stderr, "error: %s: %s\n", url.c_str(), error.c_str());
    return false;
  }
  return true;
}

static int
do_show_repomd(const options &, database &db, const char *base)
{
  std::string base_canon(base_url_canon(base));
  repomd rp;
  if (!acquire_repomd(db, base_canon.c_str(), rp)) {
    return 1;
  }
  printf("revision: %s\n", rp.revision.c_str());
  for (std::vector<repomd::entry>::iterator p = rp.entries.begin(),
	 end = rp.entries.end();
       p != end; ++p) {
    std::string entry_url(url_combine(base_canon.c_str(), p->href.c_str()));
    printf("entry: %s %s\n", p->type.c_str(), entry_url.c_str());
  }
  return 0;
}

static int
do_show_soname_conflicts(const options &opt, database &db)
{
  database::package_set_id pset = db.lookup_package_set(opt.set_name);
  if (pset > 0) {
    db.print_elf_soname_conflicts(pset, opt.output == opt.verbose);
    return 0;
  } else {
    fprintf(stderr, "error: invalid package set: %s\n", opt.set_name);
    return 1;
  }
}


static void
usage(const char *progname, const char *error = NULL)
{
  if (error) {
    fprintf(stderr, "error: %s\n", error);
  }
  fprintf(stderr, "Usage:\n\n"
	  "  %1$s --create-schema\n"
	  "  %1$s --load-rpm [OPTIONS] RPM-FILE...\n"
	  "  %1$s --create-set=NAME --arch=ARCH [OPTIONS] RPM-FILE...\n"
	  "  %1$s --update-set=NAME [OPTIONS] RPM-FILE...\n"
	  "  %1$s --download URL\n"
	  "  %1$s --show-repomd URL\n"
	  "  %1$s --show-soname-conflicts=PACKAGE-SET [OPTIONS]\n"
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
      create_schema,
      load_rpm,
      create_set,
      update_set,
      download,
      show_repomd,
      show_soname_conflicts,
    } type;
  };
}

int
main(int argc, char **argv)
{
  command::type cmd = command::undefined;
  {
    static const struct option long_options[] = {
      {"create-schema", no_argument, 0, command::create_schema},
      {"load-rpm", no_argument, 0, command::load_rpm},
      {"create-set", required_argument, 0, command::create_set},
      {"update-set", required_argument, 0, command::update_set},
      {"download", no_argument, 0, command::download},
      {"show-repomd", no_argument, 0, command::show_repomd},
      {"show-soname-conflicts", required_argument, 0,
       command::show_soname_conflicts},
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
      case command::update_set:
      case command::show_soname_conflicts:
	cmd = static_cast<command::type>(ch);
	opt.set_name = optarg;
	break;
      case command::create_schema:
      case command::load_rpm:
      case command::download:
      case command::show_repomd:
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
    case command::create_schema:
    case command::show_soname_conflicts:
      if (argc != optind) {
	usage(argv[0]);
      }
      break;
    case command::undefined:
    case command::update_set:
      break;
    case command::download:
    case command::show_repomd:
      if (argc - optind != 1) {
	usage(argv[0]);
      }
    }
  }

  elf_image_init();
  rpm_parser_init();

  db.reset(new database);

  switch (cmd) {
  case command::create_schema:
    return do_create_schema();
  case command::load_rpm:
    do_load_rpm(opt, argv + optind);
    break;
  case command::create_set:
    return do_create_set(opt, argv + optind);
  case command::update_set:
    return do_update_set(opt, argv + optind);
  case command::download:
    return do_download(opt, *db, argv[optind]);
  case command::show_repomd:
    return do_show_repomd(opt, *db, argv[optind]);
  case command::show_soname_conflicts:
    return do_show_soname_conflicts(opt, *db);
  case command::undefined:
  default:
    abort();
  }

  return 0;
}

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

#include "rpm_load.hpp"
#include "database.hpp"
#include "elf_exception.hpp"
#include "elf_image.hpp"
#include "elf_symbol_definition.hpp"
#include "elf_symbol_reference.hpp"
#include "fd_handle.hpp"
#include "fd_source.hpp"
#include "hash.hpp"
#include "os.hpp"
#include "rpm_package_info.hpp"
#include "rpm_parser.hpp"
#include "source_sink.hpp"
#include "symboldb_options.hpp"
#include "tee_sink.hpp"

#include <sstream>

#include <cstdio>

#include <inttypes.h>
#include <fcntl.h>
#include <unistd.h>

static void
dump_def(const symboldb_options &opt, database &db,
	 database::file_id fid, const char *elf_path,
	 const elf_symbol_definition &def)
{
  if (def.symbol_name.empty()) {
    return;
  }
  if (opt.output == symboldb_options::verbose) {
    fprintf(stderr, "%s DEF %s %s 0x%llx%s\n",
	    elf_path, def.symbol_name.c_str(), def.vda_name.c_str(),
	    def.value, def.default_version ? " [default]" : "");
  }
  db.add_elf_symbol_definition(fid, def);
}

static void
dump_ref(const symboldb_options &opt, database &db,
	 database::file_id fid, const char *elf_path,
	 const elf_symbol_reference &ref)
{
  if (ref.symbol_name.empty()) {
    return;
  }
  if (opt.output == symboldb_options::verbose) {
    fprintf(stderr, "%s REF %s %s\n",
	    elf_path, ref.symbol_name.c_str(), ref.vna_name.c_str());
  }
  db.add_elf_symbol_reference(fid, ref);
}

// Locks the package digest in the database.  Used to prevent
// concurrent insertion of RPMs.
static database::advisory_lock
lock_rpm(database &db, const rpm_package_info &info)
{
  return db.lock_digest(info.hash.begin(), info.hash.end());
}

static database::package_id
load_rpm_internal(const symboldb_options &opt, database &db,
		  const char *rpm_path, rpm_package_info &info)
{
  rpm_parser_state rpmst(rpm_path);
  info = rpmst.package();
  // We can destroy the lock immediately because we are running in a
  // transaction.
  lock_rpm(db, info);
  rpm_file_entry file;

  database::package_id pkg;
  if (!db.intern_package(rpmst.package(), pkg)) {
    if (opt.output != symboldb_options::quiet) {
      fprintf(stderr, "info: skipping %s from %s\n",
	      rpmst.nevra(), rpm_path);
    }
    return pkg;
  }

  if (opt.output != symboldb_options::quiet) {
    fprintf(stderr, "info: loading %s from %s\n", rpmst.nevra(), rpm_path);
  }

  while (rpmst.read_file(file)) {
    if (opt.output == symboldb_options::verbose) {
      fprintf(stderr, "%s %s %s %s %" PRIu32 " 0%o %llu\n",
	      rpmst.nevra(), file.info->name.c_str(),
	      file.info->user.c_str(), file.info->group.c_str(),
	      file.info->mtime, file.info->mode,
	      (unsigned long long)file.contents.size());
    }
    file.info->normalize_name();
    database::file_id fid = db.add_file(pkg, *file.info);
    // Check if this is an ELF file.
    if (file.contents.size() > 4
	&& file.contents.at(0) == '\x7f'
	&& file.contents.at(1) == 'E'
	&& file.contents.at(2) == 'L'
	&& file.contents.at(3) == 'F') {

      try {
	const char *elf_path = file.info->name.c_str();
	elf_image image(file.contents.data(), file.contents.size());
	{
	  elf_image::symbol_range symbols(image);
	  while (symbols.next()) {
	    if (symbols.definition()) {
	      dump_def(opt, db, fid, elf_path, *symbols.definition());
	    } else if (symbols.reference()) {
	      dump_ref(opt, db, fid, elf_path, *symbols.reference());
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
	      db.add_elf_needed(fid, dyn.value().c_str());
	      break;
	    case elf_image::dynamic_section_range::soname:
	      if (soname_seen) {
		// The linker ignores some subsequent sonames, but
		// not all of them.  Multiple sonames are rare.
		if (dyn.value() != soname) {
		  std::ostringstream out;
		  out << "duplicate soname ignored: " << dyn.value()
		      << ", previous soname: " << soname;
		  db.add_elf_error(fid, out.str().c_str());
		}
	      } else {
		soname = dyn.value();
		soname_seen = true;
	      }
	      break;
	    case elf_image::dynamic_section_range::rpath:
	      db.add_elf_rpath(fid, dyn.value().c_str());
	      break;
	    case elf_image::dynamic_section_range::runpath:
	      db.add_elf_runpath(fid, dyn.value().c_str());
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
	db.add_elf_image(fid, image, info.arch.c_str(), soname.c_str());
      } catch (elf_exception e) {
	fprintf(stderr, "%s(%s): ELF error: %s\n",
		rpm_path, file.info->name.c_str(), e.what());
	continue;
      }
    }
  }
  return pkg;
}

bool
rpm_load(const symboldb_options &opt, database &db,
	 const char *path, rpm_package_info &info)
{
  // Unreferenced RPMs should not be visible to analyzers, so we can
  // load each RPM in a separate transaction.
  db.txn_begin();
  database::package_id pkg = load_rpm_internal(opt, db, path, info);

  hash_sink sha256(hash_sink::sha256);
  hash_sink sha1(hash_sink::sha1);
  {
    fd_handle handle;
    handle.raw = ::open(path, O_RDONLY | O_CLOEXEC);
    if (handle.raw < 0) {
      fprintf(stderr, "error: opening %s: %s\n", path, error_string().c_str());
      db.txn_rollback();
      return false;
    }

    fd_source source(handle.raw);
    tee_sink tee(&sha256, &sha1);
    copy_source_to_sink(source, tee);
  }

  std::vector<unsigned char> digest;
  sha256.digest(digest);
  db.add_package_digest(pkg, digest);
  sha1.digest(digest);
  db.add_package_digest(pkg, digest);

  db.txn_commit();
  return true;
}

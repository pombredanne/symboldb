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

#include <symboldb/rpm_load.hpp>
#include <symboldb/database.hpp>
#include <symboldb/options.hpp>
#include <cxxll/elf_exception.hpp>
#include <cxxll/elf_image.hpp>
#include <cxxll/elf_symbol_definition.hpp>
#include <cxxll/elf_symbol_reference.hpp>
#include <cxxll/fd_handle.hpp>
#include <cxxll/fd_source.hpp>
#include <cxxll/hash.hpp>
#include <cxxll/os.hpp>
#include <cxxll/rpm_package_info.hpp>
#include <cxxll/rpm_parser.hpp>
#include <cxxll/rpm_parser_exception.hpp>
#include <cxxll/source_sink.hpp>
#include <cxxll/tee_sink.hpp>

#include <map>
#include <sstream>

#include <cassert>
#include <cstdio>

#include <inttypes.h>
#include <fcntl.h>
#include <unistd.h>

using namespace cxxll;

static void
dump_def(const symboldb_options &opt, database &db,
	 database::contents_id cid, const char *elf_path,
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
  db.add_elf_symbol_definition(cid, def);
}

static void
dump_ref(const symboldb_options &opt, database &db,
	 database::contents_id cid, const char *elf_path,
	 const elf_symbol_reference &ref)
{
  if (ref.symbol_name.empty()) {
    return;
  }
  if (opt.output == symboldb_options::verbose) {
    fprintf(stderr, "%s REF %s %s\n",
	    elf_path, ref.symbol_name.c_str(), ref.vna_name.c_str());
  }
  db.add_elf_symbol_reference(cid, ref);
}

// Locks the package digest in the database.  Used to prevent
// concurrent insertion of RPMs.
static database::advisory_lock
lock_rpm(database &db, const rpm_package_info &info)
{
  return db.lock_digest(info.hash.begin(), info.hash.end());
}

// Returns true if the file starts with the ELF magic bytes.
static bool
is_elf(const std::vector<unsigned char> data)
{
  return data.size() > 4
    && data.at(0) == '\x7f'
    && data.at(1) == 'E'
    && data.at(2) == 'L'
    && data.at(3) == 'F';
}

// Loads an ELF image.
static void
load_elf(const symboldb_options &opt, database &db,
	 database::contents_id cid, const rpm_file_entry &file)
{
  try {
    const char *elf_path = file.info->name.c_str();
    elf_image image(file.contents.data(), file.contents.size());
    {
      elf_image::symbol_range symbols(image);
      while (symbols.next()) {
	if (symbols.definition()) {
	  dump_def(opt, db, cid, elf_path, *symbols.definition());
	} else if (symbols.reference()) {
	  dump_ref(opt, db, cid, elf_path, *symbols.reference());
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
	  db.add_elf_needed(cid, dyn.value().c_str());
	  break;
	case elf_image::dynamic_section_range::soname:
	  if (soname_seen) {
	    // The linker ignores some subsequent sonames, but
	    // not all of them.  Multiple sonames are rare.
	    if (dyn.value() != soname) {
	      std::ostringstream out;
	      out << "duplicate soname ignored: " << dyn.value()
		  << ", previous soname: " << soname;
	      db.add_elf_error(cid, out.str().c_str());
	    }
	  } else {
	    soname = dyn.value();
	    soname_seen = true;
	  }
	  break;
	case elf_image::dynamic_section_range::rpath:
	  db.add_elf_rpath(cid, dyn.value().c_str());
	  break;
	case elf_image::dynamic_section_range::runpath:
	  db.add_elf_runpath(cid, dyn.value().c_str());
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
    db.add_elf_image(cid, image, soname.c_str());
  } catch (elf_exception e) {
    db.add_elf_error(cid, e.what());
  }
}

namespace {
  struct dentry {
    std::string name;
    bool normalized;
    explicit dentry(const rpm_file_info &info);
  };

  dentry::dentry(const rpm_file_info &info)
    : name(info.name), normalized(info.normalized)
  {
  }

  struct inode {
    rpm_file_info info;
    std::vector<dentry> entries;

    explicit inode(const rpm_file_info &i);

    void check_consistency(const rpm_file_info &new_info) const;
    void add(const rpm_file_info &new_info);
  };

  inode::inode(const rpm_file_info &i)
    : info(i)
  {
    if (info.nlinks < 2) {
      throw rpm_parser_exception("invalid link count for " + i.name);
    }
    entries.push_back(dentry(i));
  }

  void
  inode::check_consistency(const rpm_file_info &new_info) const
  {
    if (info.nlinks == entries.size()) {
      throw rpm_parser_exception
	("all inode references already seen at " + new_info.name);
    }
    if (info.length != new_info.length) {
      throw rpm_parser_exception
	("intra-inode length mismatch for " + new_info.name);
    }
    if (info.nlinks != new_info.nlinks) {
      throw rpm_parser_exception
	("intra-inode link count mismatch for " + new_info.name);
    }
    if (info.user != new_info.user) {
      throw rpm_parser_exception
	("intra-inode user mismatch for " + new_info.name);
    }
    if (info.group != new_info.group) {
      throw rpm_parser_exception
	("intra-inode user mismatch for " + new_info.name);
    }
    if (info.mtime != new_info.mtime) {
      throw rpm_parser_exception
	("intra-inode mtime mismatch for " + new_info.name);
    }
    if (info.mode != new_info.mode) {
      throw rpm_parser_exception
	("intra-inode mode mismatch for " + new_info.name);
    }
  }

  void
  inode::add(const rpm_file_info &new_info)
  {
    check_consistency(new_info);
    entries.push_back(dentry(new_info));
  }

  typedef std::map<unsigned, inode> inode_map;
}

static database::contents_id
load_contents(const symboldb_options &opt, database &db,
	      const rpm_file_entry &file)
{
  std::vector<unsigned char> digest(hash(hash_sink::sha256, file.contents));
  std::vector<unsigned char> preview
    (file.contents.begin(),
     file.contents.begin() + std::min(static_cast<size_t>(64),
				      file.contents.size()));
  database::contents_id cid;
  if (db.intern_file_contents(*file.info, digest, preview, cid)) {
    if (is_elf(file.contents)) {
      load_elf(opt, db, cid, file);
    }
  }
  return cid;
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

  inode_map inodes;

  // FIXME: We should not read arbitrary files into memory, only ELF
  // files.
  while (rpmst.read_file(file)) {
    if (opt.output == symboldb_options::verbose) {
      fprintf(stderr, "%s %s %s %s %" PRIu32 " 0%o %llu\n",
	      rpmst.nevra(), file.info->name.c_str(),
	      file.info->user.c_str(), file.info->group.c_str(),
	      file.info->mtime, file.info->mode,
	      (unsigned long long)file.contents.size());
    }
    file.info->normalize_name();
    if (file.info->is_directory()) {
      db.add_directory(pkg, *file.info);
    } else if (file.info->is_symlink()) {
      db.add_symlink(pkg, *file.info, file.contents);
    } else {
      // FIXME: deal with special files.
      unsigned ino = file.info->ino;
      if (file.info->nlinks > 1) {
	inode_map::iterator p(inodes.find(ino));
	if (p == inodes.end()) {
	  inodes.insert(std::make_pair(ino, inode(*file.info)));
	} else {
	  p->second.add(*file.info);
	  if (p->second.entries.size() == p->second.info.nlinks) {
	    // This is the last entry for this inode, and it comes
	    // with the contents.  Load it and patch in the previous
	    // references.
	    database::contents_id cid = load_contents(opt, db, file);
	    for (std::vector<dentry>::const_iterator
		   q = p->second.entries.begin(), end = p->second.entries.end();
		 q != end; ++q) {
	      db.add_file(pkg, q->name, q->normalized, ino, cid);
	    }
	  }
	}
      } else {
	// No hardlinks.
	database::contents_id cid = load_contents(opt, db, file);
	db.add_file(pkg, file.info->name, file.info->normalized, ino, cid);
      }
    }
  }
  return pkg;
}

database::package_id
rpm_load(const symboldb_options &opt, database &db,
	 const char *path, rpm_package_info &info)
{
  // Unreferenced RPMs should not be visible to analyzers, so we can
  // load each RPM in a separate transaction.  We make a synchronous
  // commit when referencing the RPM data, so a non-synchronous commit
  // is sufficient here.
  db.txn_begin_no_sync();
  database::package_id pkg = load_rpm_internal(opt, db, path, info);

  hash_sink sha256(hash_sink::sha256);
  hash_sink sha1(hash_sink::sha1);
  {
    fd_handle handle;
    handle.open_read_only(path);
    fd_source source(handle.get());
    tee_sink tee(&sha256, &sha1);
    copy_source_to_sink(source, tee);
  }
  assert(sha256.octets() == sha1.octets());

  std::vector<unsigned char> digest;
  sha256.digest(digest);
  db.add_package_digest(pkg, digest, sha256.octets());
  sha1.digest(digest);
  db.add_package_digest(pkg, digest, sha1.octets());

  db.txn_commit();
  return pkg;
}

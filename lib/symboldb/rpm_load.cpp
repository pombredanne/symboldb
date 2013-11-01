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
#include <cxxll/rpm_file_entry.hpp>
#include <cxxll/rpm_script.hpp>
#include <cxxll/rpm_trigger.hpp>
#include <cxxll/source_sink.hpp>
#include <cxxll/tee_sink.hpp>
#include <cxxll/base16.hpp>
#include <cxxll/java_class.hpp>
#include <cxxll/zip_file.hpp>
#include <cxxll/os_exception.hpp>
#include <cxxll/python_analyzer.hpp>
#include <cxxll/string_support.hpp>
#include <cxxll/vector_source.hpp>
#include <cxxll/expat_source.hpp>
#include <cxxll/maven_url.hpp>
#include <cxxll/looks_like_xml.hpp>
#include <cxxll/raise.hpp>

#include <map>
#include <sstream>

#include <cassert>
#include <cstdio>
#include <cstring>

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
    // Used for error reporting.
    const char *elf_path = file.infos.front().name.c_str();

    elf_image image(file.contents.data(), file.contents.size());
    {
      elf_image::symbol_range symbols(image);
      while (symbols.next()) {
	if (symbols.definition()) {
	  dump_def(opt, db, cid, elf_path, *symbols.definition());
	} else if (symbols.reference()) {
	  dump_ref(opt, db, cid, elf_path, *symbols.reference());
	} else {
	  raise<std::logic_error>("unknown elf_symbol type");
	}
      }
    }
    std::string soname;
    bool soname_seen = false;
    {
      elf_image::dynamic_section_range dyn(image);
      while (dyn.next()) {
	switch (dyn.type()) {
	case elf_image::dynamic_section_range::needed:
	  db.add_elf_needed(cid, dyn.text().c_str());
	  break;
	case elf_image::dynamic_section_range::soname:
	  if (soname_seen) {
	    // The linker ignores some subsequent sonames, but
	    // not all of them.  Multiple sonames are rare.
	    if (dyn.text() != soname) {
	      std::ostringstream out;
	      out << "duplicate soname ignored: " << dyn.text()
		  << ", previous soname: " << soname;
	      db.add_elf_error(cid, out.str().c_str());
	    }
	  } else {
	    soname = dyn.text();
	    soname_seen = true;
	  }
	  break;
	case elf_image::dynamic_section_range::rpath:
	  db.add_elf_rpath(cid, dyn.text().c_str());
	  break;
	case elf_image::dynamic_section_range::runpath:
	  db.add_elf_runpath(cid, dyn.text().c_str());
	  break;
	case elf_image::dynamic_section_range::other:
	  {
	    long long tag = dyn.tag();
	    long long number = dyn.number();
	    // Skip NULL entries.
	    if (tag != 0 || number != 0) {
	      db.add_elf_dynamic(cid, dyn.tag(), dyn.number());
	    }
	  }
	  break;
	}
      }
    }
    // We used to derive the soname from the file name, but because of
    // hardlinks (and deduplication), we no longer can do this here.
    const char *sonameptr = soname_seen ? soname.c_str() : nullptr;
    db.add_elf_image(cid, image, sonameptr);
  } catch (elf_exception e) {
    db.add_elf_error(cid, e.what());
  }
}

// Returns true if the file looks like a Python program.
// (Due to lack of a clear signature, we also need to test the file name.)
static bool
is_python(const std::vector<unsigned char> data)
{
  return data.size() > 10 && data.at(0) == '#'
    && memmem(data.data(), std::min<size_t>(data.size(), 100U), "python", 6U) != 0;
}

static bool
is_python_path(const rpm_file_info &info)
{
  return ends_with(info.name, ".py");
}

// Loads python source code.
static void
load_python(const symboldb_options &, database &db, python_analyzer &pya,
	    database::contents_id cid, const rpm_file_entry &file)
{
  if (db.has_python_analysis(cid)) {
    return;
  }
  if (!pya.parse(file.contents)) {
    db.add_python_error(cid, pya.error_line(), pya.error_message().c_str());
    return;
  }
  for (std::vector<std::string>::const_iterator
	 p = pya.imports().begin(), end = pya.imports().end(); p != end; ++p) {
    db.add_python_import(cid, p->c_str());
  }
  for (std::vector<std::string>::const_iterator
	 p = pya.attributes().begin(), end = pya.attributes().end();
       p != end; ++p) {
    db.add_python_attribute(cid, p->c_str());
  }
  for (std::vector<std::string>::const_iterator
	 p = pya.functions().begin(), end = pya.functions().end();
       p != end; ++p) {
    db.add_python_function_def(cid, p->c_str());
  }
  for (std::vector<std::string>::const_iterator
	 p = pya.classes().begin(), end = pya.classes().end();
       p != end; ++p) {
    db.add_python_class_def(cid, p->c_str());
  }
}

// Loads XML-like content.
static void
load_xml(const symboldb_options &, database &db,
	 database::contents_id cid, const rpm_file_entry &file)
{
  vector_source src(&file.contents);
  expat_source source(&src);

  // Extract URLs from POM files.
  std::vector<maven_url> result;
  try {
    maven_url::extract(source, result);
  } catch (expat_source::malformed &e) {
    std::vector<unsigned char> before(e.before().begin(), e.before().end());
    std::vector<unsigned char> after(e.after().begin(), e.after().end());
    db.add_xml_error(cid, e.message(), e.line(), before, after);
  }
  for (std::vector<maven_url>::const_iterator
	 p = result.begin(), end = result.end();
       p != end; ++p) {
      db.add_maven_url(cid, *p);
  }
}

static const size_t FILE_CONTENTS_PREVIEW_SIZE = 64;

// Return true if the full contents of the file should be preserved in
// the database.  (Usually, only the first FILE_CONTENTS_PREVIEW_SIZE
// bytes will be stored.)  Affected files are configuration and
// service registration files.
static bool
keep_full_contents(const rpm_file_info &info)
{
  const std::string &path(info.name);
  return starts_with(path, "/etc/")
    || starts_with(path, "/usr/lib/binfmt.d/")
    || starts_with(path, "/usr/lib/sysctl.d/")
    || starts_with(path, "/usr/lib/tmpfiles.d/")
    || starts_with(path, "/usr/lib/udev/rules.d/")
    || starts_with(path, "/usr/share/dbus-1/services/")
    || starts_with(path, "/usr/share/dbus-1/system-services/")
    || starts_with(path, "/usr/share/polkit-1/actions/")
    || starts_with(path, "/usr/share/polkit-1/rules.d/")
    || ends_with(path, ".conf")
    || ends_with(path, ".desktop")
    || ends_with(path, ".pkla")
    || ends_with(path, ".policy")
    || ends_with(path, ".protocol")
    || ends_with(path, ".service")
    || ends_with(path, ".spec")
    ;
}

static bool
check_any(const std::vector<rpm_file_info> &infos,
	  bool (*func)(const rpm_file_info &))
{
  typedef std::vector<rpm_file_info>::const_iterator iterator;
  const iterator end = infos.end();
  for (iterator p = infos.begin(); p != end; ++p) {
    if (func(*p)) {
      return true;
    }
  }
  return false;
}

static void
update_contents_preview(const rpm_file_entry &file,
			std::vector<unsigned char> &preview)
{
  if (check_any(file.infos, keep_full_contents)) {
    preview = file.contents;
  } else {
    size_t restricted_size = std::min(FILE_CONTENTS_PREVIEW_SIZE,
				      file.contents.size());
    preview.assign(file.contents.begin(),
		   file.contents.begin() + restricted_size);
  }
}

static void
check_digest(const char *rpm_path, const std::string &file,
	     const checksum &actual, const checksum &expected)
{
  if (actual.value != expected.value) {
    raise<std::runtime_error>
      (std::string(rpm_path)
       + ": digest mismatch for "
       + file + " (actual "
       + base16_encode(actual.value.begin(), actual.value.end())
       + ", expected "
       + base16_encode(expected.value.begin(), expected.value.end())
       + ")");
  }
}

static void
prepare_load(const char *rpm_path, const rpm_file_entry &file,
	     std::vector<unsigned char> &digest,
	     std::vector<unsigned char> &preview)
{
  checksum csum;
  csum.type = hash_sink::sha256;
  csum.value = hash(hash_sink::sha256, file.contents);
  // We only need to check the first file, our RPM parser
  // has performed the internal consistency check.
  const rpm_file_info &info = file.infos.front();
  if (info.digest.type == hash_sink::sha256) {
    check_digest(rpm_path, info.name, csum, info.digest);
  } else {
    checksum chk_csum;
    chk_csum.type = info.digest.type;
    chk_csum.value = hash(chk_csum.type, file.contents);
    check_digest(rpm_path, info.name, chk_csum, info.digest);
  }
  std::swap(digest, csum.value);
  update_contents_preview(file, preview);
}

static void
load_zip(database &db,
	 database::contents_id cid, const rpm_file_entry &file)
{
  zip_file zip(&file.contents);
  std::vector<unsigned char> data;
  while (true) {
    try {
      if (!zip.next()) {
	break;
      }
    } catch (os_exception &e) {
      // FIXME: Use proper, zip_file-specific exception.
      // zip.name() is not necessarily valid at this point.
      db.add_java_error(cid, e.what(), "");
      // Exit the loop because the file is likely corrupted significantly.
      break;
    }
    try {
      zip.data(data);
    } catch (os_exception &e) {
      // FIXME: Use proper, zip_file-specific exception.
      db.add_java_error(cid, e.what(), zip.name());
    }
    if (java_class::has_signature(data)) {
      try {
	java_class jc(&data);
	db.add_java_class(cid, jc);
      } catch (java_class::exception &e) {
	db.add_java_error(cid, e.what(), zip.name());
      }
    }
  }
}

static void
do_load_formats(const symboldb_options &opt, database &db, python_analyzer &pya,
		database::contents_id cid, const rpm_file_entry &file)
{
  if (is_elf(file.contents)) {
    load_elf(opt, db, cid, file);
  } else if (looks_like_xml(file.contents.begin(), file.contents.end())) {
    load_xml(opt, db, cid, file);
  } else if (is_python(file.contents)
	     || check_any(file.infos, is_python_path)) {
    load_python(opt, db, pya, cid, file);
  } else if (java_class::has_signature(file.contents)) {
    try {
      java_class jc(&file.contents);
      db.add_java_class(cid, jc);
    } catch (java_class::exception &e) {
      db.add_java_error(cid, e.what(), "");
    }
  }
  if (zip_file::has_signature(file.contents)) {
    load_zip(db, cid, file);
  }
}

static inline bool
unpack_files(const rpm_package_info &pkginfo)
{
  return pkginfo.kind == rpm_package_info::binary;
}

static void
add_files(const symboldb_options &opt, database &db, python_analyzer &pya,
	  const rpm_package_info &pkginfo, database::package_id pkg,
	  const char *rpm_path, rpm_file_entry &file)
{
  std::vector<unsigned char> digest;
  std::vector<unsigned char> preview;
  prepare_load(rpm_path, file, digest, preview);

  file.infos.front().normalize_name();

  database::file_id fid;
  database::contents_id cid;
  database::attribute_id aid;
  bool added;
  int contents_length;
  db.add_file(pkg, file.infos.front(), digest, preview, fid, cid,
	      added, contents_length);

  bool looks_like_python =
    unpack_files(pkginfo) && is_python_path(file.infos.front());

  if (file.infos.size() > 1) {
    assert(!file.infos.front().ghost());
    for (std::vector<rpm_file_info>::iterator
	 p = file.infos.begin() + 1, end = file.infos.end();
	 p != end; ++p) {
      assert(!p->ghost());
      p->normalize_name();
      aid = db.intern_file_attribute(*p);
      db.add_file(pkg, p->name, p->mtime, p->ino, cid, aid);
      looks_like_python = looks_like_python
	|| (unpack_files(pkginfo) && is_python_path(file.infos.front()));
    }
  }

  if (added) {
    if (unpack_files(pkginfo)) {
      do_load_formats(opt, db, pya, cid, file);
    }
  } else {
    // We might recognize additonal files as Python files if they are
    // loaded later under a different name.
    if (looks_like_python) {
      load_python(opt, db, pya, cid, file);
    }
  }
  // If the contents in the database was truncated, update it with the
  // longer version.
  if (static_cast<size_t>(contents_length) < preview.size()) {
    db.update_contents_preview(cid, preview);
  }
}

static void
dependencies(const symboldb_options &, database &db,
	     database::package_id pkg, rpm_parser &rpmparser)
{
  const std::vector<rpm_dependency> &deps(rpmparser.dependencies());
  for (std::vector<rpm_dependency>::const_iterator
	 p = deps.begin(), end = deps.end(); p != end; ++p) {
    db.add_package_dependency(pkg, *p);
  }
}

static void
scripts(const symboldb_options &, database &db,
	database::package_id pkg, rpm_parser &rpmparser)
{
  std::vector<rpm_script> scripts;
  rpmparser.scripts(scripts);
  for (std::vector<rpm_script>::const_iterator
	 p = scripts.begin(), end = scripts.end(); p != end; ++p) {
    db.add_package_script(pkg, *p);
  }
}

static void
triggers(const symboldb_options &, database &db,
	 database::package_id pkg, rpm_parser &rpmparser)
{
  std::vector<rpm_trigger> triggers;
  rpmparser.triggers(triggers);
  int i = 0;
  for (std::vector<rpm_trigger>::const_iterator
	 p = triggers.begin(), end = triggers.end(); p != end; ++p) {
    db.add_package_trigger(pkg, *p, i);
    ++i;
  }
}

static database::package_id
load_rpm_internal(const symboldb_options &opt, database &db,
		  const char *rpm_path, rpm_package_info &pkginfo)
{
  rpm_parser rpmparser(rpm_path);
  python_analyzer pya;
  pkginfo = rpmparser.package();
  // We can destroy the lock immediately because we are running in a
  // transaction.
  lock_rpm(db, pkginfo);
  rpm_file_entry file;

  database::package_id pkg;
  if (!db.intern_package(rpmparser.package(), pkg)) {
    if (opt.output != symboldb_options::quiet) {
      fprintf(stderr, "info: skipping %s from %s\n",
	      rpmparser.nevra(), rpm_path);
    }
    return pkg;
  }

  if (opt.output != symboldb_options::quiet) {
    fprintf(stderr, "info: loading %s from %s\n", rpmparser.nevra(), rpm_path);
  }

  dependencies(opt, db, pkg, rpmparser);
  scripts(opt, db, pkg, rpmparser);
  triggers(opt, db, pkg, rpmparser);

  // FIXME: We should not read arbitrary files into memory, only ELF
  // files.
  while (rpmparser.read_file(file)) {
    if (file.infos.size() > 1) {
      // Hard links, so this is a real file.
      add_files(opt, db, pya, pkginfo, pkg, rpm_path, file);
      continue;
    }

    assert(!file.infos.empty());
    rpm_file_info &info(file.infos.front());
    if (info.is_directory()) {
      db.add_directory(pkg, info);
    } else if (info.is_symlink()) {
      db.add_symlink(pkg, info);
    } else {
      add_files(opt, db, pya, pkginfo, pkg, rpm_path, file);
    }
  }
  return pkg;
}

database::package_id
rpm_load(const symboldb_options &opt, database &db,
	 const char *path, rpm_package_info &info,
	 const checksum *expected, const char *url)
{
  if (expected && (expected->type != hash_sink::sha256
		   && expected->type != hash_sink::sha1)) {
    raise<std::runtime_error>("unsupported hash type");
  }

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
  if (expected && expected->type == hash_sink::sha256
      && expected->value != digest) {
    raise<std::runtime_error>("checksum mismatch");
  }
  sha1.digest(digest);
  db.add_package_digest(pkg, digest, sha1.octets());
  if (expected && expected->type == hash_sink::sha1
      && expected->value != digest) {
    raise<std::runtime_error>("checksum mismatch");
  }
  if (url != nullptr) {
    db.add_package_url(pkg, url);
  }

  db.txn_commit();
  return pkg;
}

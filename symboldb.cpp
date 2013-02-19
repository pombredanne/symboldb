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
#include "string_support.hpp"
#include "zlib.hpp"
#include "expat_minidom.hpp"
#include "os.hpp"
#include "file_cache.hpp"
#include "hash.hpp"

#include <set>
#include <sstream>

namespace {
  struct options {
    enum {
      standard, verbose, quiet
    } output;

    const char *arch;
    bool no_net;
    const char *set_name;
    std::string cache_path;

    options()
      : output(standard), arch(NULL), no_net(false), set_name(NULL)
    {
    }

    download_options download() const;

    std::string rpm_cache_path() const;
  };

  download_options
  options::download() const
  {
    download_options d;
    if (no_net) {
      d.cache_mode = download_options::only_cache;
    }
    return d;
  }

  std::string
  options::rpm_cache_path() const
  {
    std::string path;
    if (cache_path.empty()) {
      path = home_directory();
      path += "/.cache/symboldb";
    } else {
      path = cache_path;
    }
    path += "/rpms";
    return path;
  }
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

static bool
load_rpm(database &db, const char *path, rpm_package_info &info)
{
  // Unreferenced RPMs should not be visible to analyzers, so we can
  // load each RPM in a separate transaction.
  db.txn_begin();
  database::package_id pkg = load_rpm(path, info);

  std::vector<unsigned char> digest;
  std::string error;
  if (!hash_sha256_file(path, digest, error)) {
    fprintf(stderr, "error: hashing %s: %s\n", path, error.c_str());
    return false;
  }
  db.add_package_sha256(pkg, digest);

  db.txn_commit();
  return true;
}

static bool
load_rpms(char **argv, package_set_consolidator<database::package_id> &ids)
{
  rpm_package_info info;
  for (; *argv; ++argv) {
    database::package_id pkg = load_rpm(*db, *argv, info);
    if (pkg == 0) {
      return false;
    }
    ids.add(info, pkg);
  }
  return true;
}

static int do_create_schema()
{
  db->create_schema();
  return 0;
}

static int
do_load_rpm(const options &opt, char **argv)
{
  package_set_consolidator<database::package_id> ignored;
  if (!load_rpms(argv, ignored)) {
    return 1;
  }
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
    package_set_consolidator<database::package_id> psc;
    if (!load_rpms(argv, psc)) {
      return 1;
    }
    ids = psc.values();
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
    package_set_consolidator<database::package_id> psc;
    if (!load_rpms(argv, psc)) {
      return 1;
    }
    ids = psc.values();
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
do_download(const options &opt, database &db, const char *url)
{
  std::vector<unsigned char> data;
  std::string error;
  if (!download(opt.download(), db, url, data, error)) {
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

static int
do_show_repomd(const options &opt, database &db, const char *base)
{
  repomd rp;
  std::string error;
  if (!rp.acquire(opt.download(), db, base, error)) {
    fprintf(stderr, "error: %s: %s\n", base, error.c_str());
    return 1;
  }
  printf("revision: %s\n", rp.revision.c_str());
  for (std::vector<repomd::entry>::iterator p = rp.entries.begin(),
	 end = rp.entries.end();
       p != end; ++p) {
    std::string entry_url(url_combine(rp.base_url.c_str(), p->href.c_str()));
    printf("entry: %s %s\n", p->type.c_str(), entry_url.c_str());
  }
  return 0;
}

namespace {
  struct rpm_url {
    std::string href;
    checksum csum;
  };
}

static int
do_download_repo(const options &opt, database &db, char **argv, bool load)
{
  // TODO: only download the latest version from all repos (at least
  // by default).

  std::string fcache_path(opt.rpm_cache_path().c_str());
  if (!make_directory_hierarchy(fcache_path.c_str(), 0700)) {
    fprintf(stderr, "error: could not create cache directory: %s\n",
	    fcache_path.c_str());
    return 1;
  }
  file_cache fcache(fcache_path.c_str());
  if (!fcache.valid()) {
    fprintf(stderr, "error: invalid RPM cache: %s\n", fcache_path.c_str());
    return 1;
  }

  package_set_consolidator<rpm_url> pset;

  for (; *argv; ++ argv) {
    const char *url = *argv;
    if (opt.output == options::verbose) {
      fprintf(stderr, "info: processing %s\n", url);
    }
    repomd rp;
    std::string error;
    if (!rp.acquire(opt.download(), db, url, error)) {
      fprintf(stderr, "error: %s: %s\n", url, error.c_str());
      return 1;
    }

    // FIXME: consolidate with do_show_source_packages() below.

    // The metadata URLs include hashes, so we do not have to check
    // the cache for staleness.  But --no-net overrides that.
    download_options dopts;
    if (opt.no_net) {
      dopts = opt.download();
    } else {
      dopts.cache_mode = download_options::always_cache;
    }

    bool found = false;
    for (std::vector<repomd::entry>::iterator p = rp.entries.begin(),
	   end = rp.entries.end();
	 p != end; ++p) {
      if (p->type == "primary" && ends_with(p->href, ".xml.gz")) {
	std::string entry_url(url_combine(rp.base_url.c_str(), p->href.c_str()));
	std::vector<unsigned char> data, uncompressed;
	if (!download(dopts, db, entry_url.c_str(), data, error)) {
	  fprintf(stderr, "error: %s (from %s): %s\n",
		  entry_url.c_str(), url, error.c_str());
	  return 1;
	}
	if (!gzip_uncompress(data, uncompressed)) {
	  fprintf(stderr, "error: %s (from %s): gzip decompression error\n",
		  entry_url.c_str(), url);
	  return 1;
	}
	if (uncompressed.empty()) {
	  fprintf(stderr, "error: %s (from %s): no data\n",
		  entry_url.c_str(), url);
	  return 1;
	}
	found = true;
	using namespace expat_minidom;
	std::tr1::shared_ptr<element> root(parse(&uncompressed.front(),
						 uncompressed.size(), error));
	if (!root) {
	  fprintf(stderr, "error: %s (from %s): XML error: %s\n",
		  entry_url.c_str(), url, error.c_str());
	  return 1;
	}
	if (root->name != "metadata") {
	  fprintf(stderr, "error: %s (from %s): invalid XML root element: %s\n",
		  entry_url.c_str(), url, root->name.c_str());
	  return 1;
	}
	for(std::vector<std::tr1::shared_ptr<node> >::iterator
	      p = root->children.begin(), end = root->children.end(); p != end; ++p) {
	  element *e = dynamic_cast<element *>(p->get());
	  if (e && e->name == "package" && e->attributes["type"] == "rpm") {
	    rpm_package_info rpminfo;
	    {
	      element *name = e->first_child("name");
	      if (!name) {
		fprintf(stderr, "error: %s (from %s): missing name element\n",
			entry_url.c_str(), url);
		return 1;
	      }
	      rpminfo.name = strip(name->text());
	    }
	    {
	      element *version = e->first_child("version");
	      if (!version) {
		fprintf(stderr, "error: %s (from %s): %s: missing version element\n",
			entry_url.c_str(), url, rpminfo.name.c_str());
		return 1;
	      }
	      unsigned long long epoch;
	      rpminfo.version = strip(version->attributes["ver"]);
	      rpminfo.release = strip(version->attributes["rel"]);
	      if (!parse_unsigned_long_long(strip(version->attributes["epoch"]),
					    epoch)
		  || epoch != static_cast<unsigned long long>(static_cast<int>(epoch))
		  || rpminfo.version.empty() || rpminfo.release.empty()) {
		fprintf(stderr, "error: %s (from %s): %s: malformed version element\n",
			entry_url.c_str(), url, rpminfo.name.c_str());
		return 1;
	      }
	      rpminfo.epoch = epoch;
	    }
	    {
	      element *arch = e->first_child("arch");
	      if (!arch) {
		fprintf(stderr, "error: %s (from %s): missing arch element\n",
			entry_url.c_str(), url);
		return 1;
	      }
	      rpminfo.arch = strip(arch->text());
	    }

	    element *format = e->first_child("format");
	    if (!format) {
	      fprintf(stderr, "error: %s (from %s): %s: missing format element\n",
		      entry_url.c_str(), url, rpminfo.name.c_str());
	      return 1;
	    }

	    element *location = e->first_child("location");
	    if (!location) {
	      fprintf(stderr, "error: %s (from %s): %s: missing location element\n",
		      entry_url.c_str(), url, rpminfo.name.c_str());
	      return 1;
	    }
	    std::string location_href = location->attributes["href"];
	    if (location_href.empty()) {
	      fprintf(stderr, "error: %s (from %s): %s: missing href attribute\n",
		      entry_url.c_str(), url, rpminfo.name.c_str());
	      return 1;
	    }
	    element *checksum_elem = e->first_child("checksum");
	    if (!checksum_elem) {
	      fprintf(stderr, "error: %s (from %s): %s: missing checksum element\n",
		      entry_url.c_str(), url, rpminfo.name.c_str());
	      return 1;
	    }
	    element *size = e->first_child("size");
	    if (!size) {
	      fprintf(stderr, "error: %s (from %s): %s: missing size element\n",
		      entry_url.c_str(), url, rpminfo.name.c_str());
	      return 1;
	    }
	    unsigned long long nsize;
	    if (!parse_unsigned_long_long(size->attributes["package"], nsize)) {
	      fprintf(stderr, "error: %s (from %s): %s: malformed size element\n",
		      entry_url.c_str(), url, rpminfo.name.c_str());
	      return 1;
	    }

	    rpm_url rurl;
	    rurl.href = url_combine(rp.base_url.c_str(),
				    location_href.c_str());
	    if (!rurl.csum.set_hexadecimal
		(checksum_elem->attributes["type"].c_str(), nsize,
		 checksum_elem->text().c_str())) {
	      fprintf(stderr, "error: %s (from %s): %s: malformed checksum element: [%s]\n",
		      entry_url.c_str(), url, rpminfo.name.c_str(),
		      checksum_elem->text().c_str());
	      return 1;
	    }

	    pset.add(rpminfo, rurl);
	  }
	}
      }
    }
    if (!found) {
      fprintf(stderr, "warning: %s: no suitable primary data found\n", url);
    }
  }

  // Cache bypass for RPM downloads.
  download_options dopts_no_cache;
  dopts_no_cache.cache_mode = download_options::no_cache;
  std::string error;

  std::vector<rpm_url> urls(pset.values());
  if (opt.output == options::verbose) {
    fprintf(stderr, "info: %zu packages in download set\n", urls.size());
  }
  size_t skipped = 0;
  std::set<database::package_id> pids;

  for (std::vector<rpm_url>::iterator p = urls.begin(), end = urls.end();
       p != end; ++p) {
    if (load) {
      database::package_id pid = db.package_by_sha256(p->csum.value);
      if (pid != 0) {
	++skipped;
	pids.insert(pid);
	continue;
      }
    }

    std::string rpm_path;
    if (fcache.lookup_path(p->csum, rpm_path)) {
      ++skipped;
    } else {
      if (opt.output == options::verbose) {
	fprintf(stderr, "info: downloading %s\n", p->href.c_str());
      }
      // FIXME: Incoming data should be streamed to disk.
      std::vector<unsigned char> data;
      if (!download(dopts_no_cache, db, p->href.c_str(), 
		    data, error)) {
	fprintf(stderr, "error: %s: %s\n",
		p->href.c_str(), error.c_str());
	return 1;
      }
      if (!fcache.add(p->csum, data, rpm_path, error)) {
	fprintf(stderr, "error: %s: addding to cache: %s\n",
		p->href.c_str(), error.c_str());
	return 1;
      }
    }

    if (load) {
      rpm_package_info info;
      database::package_id pid = load_rpm(db, rpm_path.c_str(), info);
      if (pid == 0) {
	return 1;
      }
      pids.insert(pid);
    }
  }
  if (opt.output == options::verbose) {
    fprintf(stderr, "info: %zu downloads found in cache\n", urls.size());
  }

  return 0;
}

static int
do_show_source_packages(const options &opt, database &db, char **argv)
{
  std::set<std::string> source_packages;
  for (; *argv; ++argv) {
    const char *url = *argv;
    if (opt.output == options::verbose) {
      fprintf(stderr, "info: processing %s\n", url);
    }
    repomd rp;
    std::string error;
    if (!rp.acquire(opt.download(), db, url, error)) {
      fprintf(stderr, "error: %s: %s\n", url, error.c_str());
      return 1;
    }

    // The metadata URLs include hashes, so we do not have to check
    // the cache for staleness.  But --no-net overrides that.
    download_options dopts;
    if (opt.no_net) {
      dopts = opt.download();
    } else {
      dopts.cache_mode = download_options::always_cache;
    }

    bool found = false;
    for (std::vector<repomd::entry>::iterator p = rp.entries.begin(),
	   end = rp.entries.end();
	 p != end; ++p) {
      if (p->type == "primary" && ends_with(p->href, ".xml.gz")) {
	std::string entry_url(url_combine(rp.base_url.c_str(), p->href.c_str()));
	std::vector<unsigned char> data, uncompressed;
	std::string error;
	if (!download(dopts, db, entry_url.c_str(), data, error)) {
	  fprintf(stderr, "error: %s (from %s): %s\n",
		  entry_url.c_str(), *argv, error.c_str());
	  return 1;
	}
	if (!gzip_uncompress(data, uncompressed)) {
	  fprintf(stderr, "error: %s (from %s): gzip decompression error\n",
		  entry_url.c_str(), *argv);
	  return 1;
	}
	if (uncompressed.empty()) {
	  fprintf(stderr, "error: %s (from %s): no data\n",
		  entry_url.c_str(), *argv);
	  return 1;
	}
	found = true;
	using namespace expat_minidom;
	std::tr1::shared_ptr<element> root(parse(&uncompressed.front(),
						 uncompressed.size(), error));
	if (!root) {
	  fprintf(stderr, "error: %s (from %s): XML error: %s\n",
		  entry_url.c_str(), *argv, error.c_str());
	  return 1;
	}
	if (root->name != "metadata") {
	  fprintf(stderr, "error: %s (from %s): invalid XML root element: %s\n",
		  entry_url.c_str(), *argv, root->name.c_str());
	  return 1;
	}
	for(std::vector<std::tr1::shared_ptr<node> >::iterator
	      p = root->children.begin(), end = root->children.end(); p != end; ++p) {
	  element *e = dynamic_cast<element *>(p->get());
	  if (e && e->name == "package" && e->attributes["type"] == "rpm") {
	    element *name = e->first_child("name");
	    if (!name) {
	      fprintf(stderr, "error: %s (from %s): missing name element\n",
		      entry_url.c_str(), *argv);
	      return 1;
	    }
	    element *format = e->first_child("format");
	    if (!format) {
	      fprintf(stderr, "error: %s (from %s): %s: missing format element\n",
		      entry_url.c_str(), *argv, name->text().c_str());
	      return 1;
	    }
	    element *source = format->first_child("rpm:sourcerpm");
	    if (!source) {
	      fprintf(stderr, "error: %s (from %s): %s: missing sourcerpm element\n",
		      entry_url.c_str(), *argv, name->text().c_str());
	      return 1;
	    }
	    std::string src(source->text());
	    size_t dash = src.rfind('-');
	    if (dash != std::string::npos) {
	      src.resize(dash);	// strip release, architecture
	      dash = src.rfind('-');
	      if (dash != std::string::npos) {
		src.resize(dash); // strip version
	      }
	    }
	    if (dash == std::string::npos) {
	      fprintf(stderr, "error: %s (from %s): malformed source RPM element: %s\n",
		      entry_url.c_str(), *argv, source->text().c_str());
	      return 1;
	    }
	    source_packages.insert(src);
	  }
	}
      }
    }
    if (!found) {
      fprintf(stderr, "warning: %s: no suitable primary data found\n", *argv);
    }
  }
  for (std::set<std::string>::const_iterator
	 p = source_packages.begin(), end = source_packages.end();
       p != end; ++p) {
    printf("%s\n", p->c_str());
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
	  "  %1$s --download [OPTIONS] URL\n"
	  "  %1$s --show-repomd [OPTIONS] URL\n"
	  "  %1$s --download-repo [OPTIONS] URL...\n"
	  "  %1$s --show-source-packages [OPTIONS] URL...\n"
	  "  %1$s --show-soname-conflicts=PACKAGE-SET [OPTIONS]\n"
	  "\nOptions:\n"
	  "  --arch=ARCH, -a   base architecture\n"
	  "  --quiet, -q       less output\n"
	  "  --cache=DIR, -C   path to the cache (default: ~/.cache/symboldb)\n"
	  "  --no-net, -N      disable most network access\n"
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
      download_repo,
      load_repo,
      show_repomd,
      show_source_packages,
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
      {"download-repo", no_argument, 0, command::download_repo},
      {"load-repo", no_argument, 0, command::load_repo},
      {"show-repomd", no_argument, 0, command::show_repomd},
      {"show-source-packages", no_argument, 0, command::show_source_packages},
      {"show-soname-conflicts", required_argument, 0,
       command::show_soname_conflicts},
      {"arch", required_argument, 0, 'a'},
      {"cache", required_argument, 0, 'C'},
      {"no-net", no_argument, 0, 'N'},
      {"verbose", no_argument, 0, 'v'},
      {"quiet", no_argument, 0, 'q'},
      {0, 0, 0, 0}
    };
    int ch;
    int index;
    while ((ch = getopt_long(argc, argv, "a:NC:qv", long_options, &index)) != -1) {
      switch (ch) {
      case 'a':
	opt.arch = optarg;
	break;
      case 'N':
	opt.no_net = true;
	break;
      case 'C':
	opt.cache_path = optarg;
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
      case command::download_repo:
      case command::load_repo:
      case command::show_repomd:
      case command::show_source_packages:
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
    case command::show_source_packages:
    case command::download_repo:
    case command::load_repo:
      if (argc == optind) {
	usage(argv[0]);
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
  case command::download_repo:
    return do_download_repo(opt, *db, argv + optind, false);
  case command::load_repo:
    return do_download_repo(opt, *db, argv + optind, true);
  case command::show_repomd:
    return do_show_repomd(opt, *db, argv[optind]);
  case command::show_source_packages:
    return do_show_source_packages(opt, *db, argv + optind);
  case command::show_soname_conflicts:
    return do_show_soname_conflicts(opt, *db);
  case command::undefined:
  default:
    abort();
  }

  return 0;
}

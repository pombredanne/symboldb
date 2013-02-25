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

#include "elf_image.hpp"
#include "rpm_parser.hpp"
#include "rpm_package_info.hpp"
#include "rpm_load.hpp"
#include "database.hpp"
#include "package_set_consolidator.hpp"
#include "repomd.hpp"
#include "download.hpp"
#include "url.hpp"
#include "string_support.hpp"
#include "file_cache.hpp"
#include "fd_sink.hpp"
#include "source_sink.hpp"
#include "pg_exception.hpp"
#include "symboldb_options.hpp"
#include "os.hpp"

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <set>

static bool
load_rpms(const symboldb_options &opt, database &db, char **argv,
	  package_set_consolidator<database::package_id> &ids)
{
  rpm_package_info info;
  for (; *argv; ++argv) {
    database::package_id pkg = rpm_load(opt, db, *argv, info);
    if (pkg == 0) {
      return false;
    }
    ids.add(info, pkg);
  }
  return true;
}

static int do_create_schema(database &db)
{
  db.create_schema();
  return 0;
}

static int
do_load_rpm(const symboldb_options &opt, database &db, char **argv)
{
  package_set_consolidator<database::package_id> ignored;
  if (!load_rpms(opt, db, argv, ignored)) {
    return 1;
  }
  return 0;
}

// Lock namespace for package sets.
enum { PACKAGE_SET_LOCK_TAG = 1667369644 };

static void
finalize_package_set(const symboldb_options &opt, database &db,
		     database::package_set_id set)
{
  if (opt.output != symboldb_options::quiet) {
    fprintf(stderr, "info: updating package set caches\n");
  }
  db.update_package_set_caches(set);
}

static int
do_create_set(const symboldb_options &opt, database &db, char **argv)
{
  if (db.lookup_package_set(opt.set_name.c_str()) != 0){
    fprintf(stderr, "error: package set \"%s\" already exists\n",
	    opt.set_name.c_str());
    return 1;
  }

  typedef std::vector<database::package_id> pset;
  pset ids;
  {
    package_set_consolidator<database::package_id> psc;
    if (!load_rpms(opt, db, argv, psc)) {
      return 1;
    }
    ids = psc.values();
  }

  db.txn_begin();
  database::package_set_id set =
    db.create_package_set(opt.set_name.c_str(), opt.arch.c_str());
  {
    database::advisory_lock lock(db.lock(PACKAGE_SET_LOCK_TAG, set));
    for (pset::const_iterator p = ids.begin(), end = ids.end(); p != end; ++p) {
      db.add_package_set(set, *p);
    }
    finalize_package_set(opt, db, set);
  }
  db.txn_commit();
  return 0;
}

static int
do_update_set(const symboldb_options &opt, database &db, char **argv)
{
  database::package_set_id set = db.lookup_package_set(opt.set_name.c_str());
  if (set == 0) {
    fprintf(stderr, "error: package set \"%s\" does not exist\n",
	    opt.set_name.c_str());
    return 1;
  }

  typedef std::vector<database::package_id> pset;
  pset ids;
  {
    package_set_consolidator<database::package_id> psc;
    if (!load_rpms(opt, db, argv, psc)) {
      return 1;
    }
    ids = psc.values();
  }

  db.txn_begin();
  {
    database::advisory_lock lock(db.lock(PACKAGE_SET_LOCK_TAG, set));
    db.empty_package_set(set);
    for (pset::const_iterator p = ids.begin(), end = ids.end(); p != end; ++p) {
      db.add_package_set(set, *p);
    }
    finalize_package_set(opt, db, set);
  }
  db.txn_commit();
  return 0;
}

static int
do_download(const symboldb_options &opt, database &db, const char *url)
{
  std::vector<unsigned char> data;
  std::string error;
  if (!download(opt.download(), db, url, data, error)) {
    fprintf(stderr, "error: %s: %s\n", url, error.c_str());
    return 1;
  }
  if (!data.empty()
      && fwrite(data.data(), data.size(), 1, stdout) != 1) {
    perror("fwrite");
    return 1;
  }
  return 0;
}

static int
do_show_repomd(const symboldb_options &opt, database &db, const char *base)
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

static int
do_show_primary(const symboldb_options &opt, database &db, const char *base)
{
  repomd rp;
  std::string error;
  if (!rp.acquire(opt.download(), db, base, error)) {
    fprintf(stderr, "error: %s: %s\n", base, error.c_str());
    return 1;
  }
  download_options dopts;
  if (opt.no_net) {
    dopts = opt.download();
  } else {
    dopts.cache_mode = download_options::always_cache;
  }
  repomd::primary_xml primary(rp, dopts, db);
  fd_sink out(STDOUT_FILENO);
  copy_source_to_sink(primary, out);
  return 0;
}

namespace {
  struct rpm_url {
    std::string href;
    checksum csum;
  };
}

static int
do_download_repo(const symboldb_options &opt, database &db,
		 char **argv, bool load)
{
  database::package_set_id set = 0;
  if (load && !opt.set_name.empty()) {
    set = db.lookup_package_set(opt.set_name.c_str());
    if (set == 0) {
      fprintf(stderr, "error: unknown package set: %s\n",
	      opt.set_name.c_str());
      return 1;
    }
  }

  std::string fcache_path(opt.rpm_cache_path().c_str());
  if (!make_directory_hierarchy(fcache_path.c_str(), 0700)) {
    fprintf(stderr, "error: could not create cache directory: %s\n",
	    fcache_path.c_str());
    return 1;
  }
  file_cache fcache(fcache_path.c_str());
  package_set_consolidator<rpm_url> pset;

  for (; *argv; ++ argv) {
    const char *url = *argv;
    if (opt.output != symboldb_options::quiet) {
      fprintf(stderr, "info: processing repository %s\n", url);
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

    repomd::primary_xml primary_xml(rp, dopts, db);
    repomd::primary primary(&primary_xml);
    while (primary.next()) {
      rpm_url rurl;
      rurl.href = url_combine(rp.base_url.c_str(),
			      primary.href().c_str());
      rurl.csum = primary.checksum();
      pset.add(primary.info(), rurl);
    }
  }

  // Cache bypass for RPM downloads.
  download_options dopts_no_cache;
  dopts_no_cache.cache_mode = download_options::no_cache;
  std::string error;

  std::vector<rpm_url> urls(pset.values());
  if (opt.output != symboldb_options::quiet) {
    fprintf(stderr, "info: %zu packages in download set\n", urls.size());
  }
  size_t download_count = 0;
  std::set<database::package_id> pids;

  for (std::vector<rpm_url>::iterator p = urls.begin(), end = urls.end();
       p != end; ++p) {
    if (load) {
      database::package_id pid = db.package_by_digest(p->csum.value);
      if (pid != 0) {
	if (opt.output != symboldb_options::quiet) {
	  fprintf(stderr, "info: skipping %s\n", p->href.c_str());
	}
	pids.insert(pid);
	continue;
      }
    }

    std::string rpm_path;
    try {
      database::advisory_lock lock
	(db.lock_digest(p->csum.value.begin(), p->csum.value.end()));
      if (!fcache.lookup_path(p->csum, rpm_path)) {
	if (opt.output != symboldb_options::quiet) {
	  fprintf(stderr, "info: downloading %s\n", p->href.c_str());
	}
	++download_count;
	file_cache::add_sink sink(fcache, p->csum);
	if (!download(dopts_no_cache, db, p->href.c_str(), 
		      &sink, error)) {
	  fprintf(stderr, "error: %s: %s\n",
		  p->href.c_str(), error.c_str());
	  return 1;
	}
	sink.finish(rpm_path);
      }
    } catch (file_cache::unsupported_hash &e) {
      fprintf(stderr, "error: unsupported hash for %s: %s\n",
	      p->href.c_str(), e.what());
      return 1;
    } catch (file_cache::checksum_mismatch &e) {
      fprintf(stderr, "error: checksum mismatch for %s: %s\n",
	      p->href.c_str(), e.what());
      return 1;
    }

    if (load) {
      rpm_package_info info;
      database::package_id pid = rpm_load(opt, db, rpm_path.c_str(), info);
      if (pid == 0) {
	return 1;
      }
      pids.insert(pid);
    }
  }
  if (opt.output != symboldb_options::quiet) {
    fprintf(stderr, "info: downloaded %zu of %zu packages\n",
	    download_count, urls.size());
  }
  if (load && set > 0) {
    db.txn_begin();
    database::advisory_lock lock(db.lock(PACKAGE_SET_LOCK_TAG, set));
    {
      db.empty_package_set(set);
      for (std::set<database::package_id>::iterator
	     p = pids.begin(), end = pids.end(); p != end; ++p) {
	db.add_package_set(set, *p);
      }
      finalize_package_set(opt, db, set);
    }
    db.txn_commit();
  }

  return 0;
}

static int
do_show_source_packages(const symboldb_options &opt, database &db, char **argv)
{
  std::set<std::string> source_packages;
  for (; *argv; ++argv) {
    const char *url = *argv;
    if (opt.output != symboldb_options::quiet) {
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

    repomd::primary_xml primary_xml(rp, dopts, db);
    repomd::primary primary(&primary_xml);
    while (primary.next()) {
      std::string src(primary.info().source_rpm);
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
		primary_xml.url().c_str(), *argv,
		primary.info().source_rpm.c_str());
	return 1;
      }
      source_packages.insert(src);
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
do_show_soname_conflicts(const symboldb_options &opt, database &db)
{
  database::package_set_id pset = db.lookup_package_set(opt.set_name.c_str());
  if (pset > 0) {
    db.print_elf_soname_conflicts(pset, opt.output == opt.verbose);
    return 0;
  } else {
    fprintf(stderr, "error: invalid package set: %s\n", opt.set_name.c_str());
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
	  "  %1$s --update-set-from-repo=NAME [OPTIONS] URL...\n"
	  "  %1$s --download [OPTIONS] URL\n"
	  "  %1$s --show-repomd [OPTIONS] URL\n"
	  "  %1$s --show-primary [OPTIONS] URL\n"
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
      update_set_from_repo,
      download,
      download_repo,
      load_repo,
      show_repomd,
      show_primary,
      show_source_packages,
      show_soname_conflicts,
    } type;
  };
}

int
main(int argc, char **argv)
{
  symboldb_options opt;
  command::type cmd = command::undefined;
  {
    static const struct option long_options[] = {
      {"create-schema", no_argument, 0, command::create_schema},
      {"load-rpm", no_argument, 0, command::load_rpm},
      {"create-set", required_argument, 0, command::create_set},
      {"update-set", required_argument, 0, command::update_set},
      {"update-set-from-repo", required_argument, 0,command::update_set_from_repo},
      {"download", no_argument, 0, command::download},
      {"download-repo", no_argument, 0, command::download_repo},
      {"load-repo", no_argument, 0, command::load_repo},
      {"show-repomd", no_argument, 0, command::show_repomd},
      {"show-primary", no_argument, 0, command::show_primary},
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
	if (optarg[0] == '\0') {
	  usage(argv[0], "invalid architecture name");
	}
	opt.arch = optarg;
	break;
      case 'N':
	opt.no_net = true;
	break;
      case 'C':
	opt.cache_path = optarg;
	break;
      case 'q':
	opt.output = symboldb_options::quiet;
	break;
      case 'v':
	opt.output = symboldb_options::verbose;
	break;
      case command::create_set:
      case command::update_set:
      case command::update_set_from_repo:
      case command::show_soname_conflicts:
	if (optarg[0] == '\0') {
	  usage(argv[0], "invalid package set name");
	}
	cmd = static_cast<command::type>(ch);
	opt.set_name = optarg;
	break;
      case command::create_schema:
      case command::load_rpm:
      case command::download:
      case command::download_repo:
      case command::load_repo:
      case command::show_repomd:
      case command::show_primary:
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
      if (opt.arch.empty()) {
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
    case command::update_set_from_repo:
      break;
    case command::download:
    case command::show_repomd:
    case command::show_primary:
      if (argc - optind != 1) {
	usage(argv[0]);
      }
    }
  }

  elf_image_init();
  rpm_parser_init();

  try {
    database db;

    switch (cmd) {
    case command::create_schema:
      return do_create_schema(db);
    case command::load_rpm:
      do_load_rpm(opt, db, argv + optind);
      break;
    case command::create_set:
      return do_create_set(opt, db, argv + optind);
    case command::update_set:
      return do_update_set(opt, db, argv + optind);
    case command::download:
      return do_download(opt, db, argv[optind]);
    case command::download_repo:
      return do_download_repo(opt, db, argv + optind, false);
    case command::load_repo:
    case command::update_set_from_repo:
      return do_download_repo(opt, db, argv + optind, true);
    case command::show_repomd:
      return do_show_repomd(opt, db, argv[optind]);
    case command::show_primary:
      return do_show_primary(opt, db, argv[optind]);
    case command::show_source_packages:
      return do_show_source_packages(opt, db, argv + optind);
    case command::show_soname_conflicts:
      return do_show_soname_conflicts(opt, db);
    case command::undefined:
    default:
      abort();
    }
  } catch (pg_exception &e) {
    fprintf(stderr, "error: from PostgreSQL:\n");
    dump("error: ", e, stderr);
    return 1;
  }

  return 0;
}

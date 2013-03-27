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

#include <cxxll/elf_image.hpp>
#include <cxxll/rpm_parser.hpp>
#include <cxxll/rpm_package_info.hpp>
#include <symboldb/rpm_load.hpp>
#include <symboldb/database.hpp>
#include <cxxll/package_set_consolidator.hpp>
#include <symboldb/repomd.hpp>
#include <symboldb/download.hpp>
#include <cxxll/url.hpp>
#include <cxxll/string_support.hpp>
#include <cxxll/file_cache.hpp>
#include <cxxll/fd_sink.hpp>
#include <cxxll/source_sink.hpp>
#include <cxxll/pg_exception.hpp>
#include <symboldb/options.hpp>
#include <symboldb/download_repo.hpp>
#include <symboldb/show_source_packages.hpp>
#include <symboldb/expire.hpp>
#include <cxxll/os.hpp>
#include <cxxll/base16.hpp>
#include <cxxll/curl_exception.hpp>
#include <cxxll/curl_exception_dump.hpp>
#include <cxxll/file_handle.hpp>

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <algorithm>
#include <vector>

using namespace cxxll;

static bool
load_rpms(const symboldb_options &opt, database &db, char **argv,
	  package_set_consolidator<database::package_id> &ids)
{
  rpm_package_info info;
  for (; *argv; ++argv) {
    database::package_id pkg = rpm_load(opt, db, *argv, info, NULL);
    if (pkg == database::package_id()) {
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

static int
do_create_set(const symboldb_options &opt, database &db, char **argv)
{
  if (db.lookup_package_set(opt.set_name.c_str())
      != database::package_set_id()) {
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
    db.create_package_set(opt.set_name.c_str());
  if (db.update_package_set(set, ids)) {
    finalize_package_set(opt, db, set);
  }
  db.txn_commit();
  return 0;
}

static int
do_update_set(const symboldb_options &opt, database &db, char **argv)
{
  database::package_set_id set = db.lookup_package_set(opt.set_name.c_str());
  if (set == database::package_set_id()) {
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
    database::advisory_lock lock
      (db.lock(database::PACKAGE_SET_LOCK_TAG, set.value()));
    if (db.update_package_set(set, ids)) {
      finalize_package_set(opt, db, set);
    }
  }
  db.txn_commit();
  return 0;
}

static int
do_download(const symboldb_options &opt, database &db, const char *url)
{
  std::vector<unsigned char> data;
  download(opt.download(), db, url, data);
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
  rp.acquire(opt.download(), db, base);
  printf("revision: %s\n", rp.revision.c_str());
  for (std::vector<repomd::entry>::iterator p = rp.entries.begin(),
	 end = rp.entries.end();
       p != end; ++p) {
    std::string entry_url(url_combine_yum(rp.base_url.c_str(), p->href.c_str()));
    printf("entry: %s %s\n", p->type.c_str(), entry_url.c_str());
  }
  return 0;
}

static int
do_show_primary(const symboldb_options &opt, database &db, const char *base)
{
  repomd rp;
  rp.acquire(opt.download(), db, base);
  repomd::primary_xml primary(rp, opt.download_always_cache(), db);
  fd_sink out(STDOUT_FILENO);
  copy_source_to_sink(primary, out);
  return 0;
}

static int
do_show_stale_cached_rpms(const symboldb_options &opt, database &db)
{
  typedef std::vector<std::vector<unsigned char> > digvec;
  std::tr1::shared_ptr<file_cache> fcache(opt.rpm_cache());
  digvec fcdigests;
  fcache->digests(fcdigests);
  std::sort(fcdigests.begin(), fcdigests.end());
  digvec dbdigests;
  db.referenced_package_digests(dbdigests);
  digvec result;
  std::set_difference(fcdigests.begin(), fcdigests.end(),
		      dbdigests.begin(), dbdigests.end(),
		      std::back_inserter(result));
  for (digvec::iterator p = result.begin(), end = result.end();
       p != end; ++p) {
    printf("%s\n", base16_encode(p->begin(), p->end()).c_str());
  }
  return 0;
}

static int
do_show_soname_conflicts(const symboldb_options &opt, database &db)
{
  database::package_set_id pset = db.lookup_package_set(opt.set_name.c_str());
  if (pset > database::package_set_id()) {
    db.print_elf_soname_conflicts(pset);
    return 0;
  } else {
    fprintf(stderr, "error: invalid package set: %s\n", opt.set_name.c_str());
    return 1;
  }
}

static int
do_run_example(const symboldb_options &opt, database &db, char **argv)
{
  int errors = 0;
  while (*argv) {
    file_handle file(*argv, "r");
    malloc_handle<char> lineptr;
    size_t line_length = 0;
    std::string current;
    unsigned lineno = 0;
    unsigned start_lineno;
    try {
      while (file.getline(lineptr, line_length)) {
	++lineno;
	std::string line(lineptr.get());
	bool start = current.empty();
	if (starts_with(line, "    ")) {
	  current += "      ";
	  current.append(line.begin() + 4, line.end());
	} else if (starts_with(line, "\t")) {
	  current.append(line.begin() + 1, line.end());
	} else if (!current.empty()) {
	  if (opt.output != symboldb_options::quiet) {
	    fprintf(stderr, "info: executing %s:%u\n", *argv, start_lineno);
	  }
	  db.exec_sql(current.c_str());
	  current.clear();
	  start = false;
	} else {
	  start = false;
	}
	if (start && !current.empty()) {
	  start_lineno = lineno;
	}
      }
      if (!current.empty()) {
	if (opt.output != symboldb_options::quiet) {
	  fprintf(stderr, "info: executing %s:%u\n", *argv, start_lineno);
	}
	db.exec_sql(current.c_str());
      }
    } catch (pg_exception &e) {
      dump("error: ", e, stderr);
      ++errors;
    }
    ++argv;
  }
  if (errors > 0) {
    return 1;
  } else {
    return 0;
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
"  %1$s --create-set=NAME [OPTIONS] RPM-FILE...\n"
"  %1$s --update-set=NAME [OPTIONS] RPM-FILE...\n"
"  %1$s --update-set-from-repo=NAME [OPTIONS] URL...\n"
"  %1$s --download [OPTIONS] URL\n"
"  %1$s --show-repomd [OPTIONS] URL\n"
"  %1$s --show-primary [OPTIONS] URL\n"
"  %1$s --download-repo [OPTIONS] URL...\n"
"  %1$s --load-repo [OPTIONS] URL...\n"
"  %1$s --show-source-packages [OPTIONS] URL...\n"
"  %1$s --show-stale-cached-rpms [OPTIONS]\n"
"  %1$s --show-soname-conflicts=PACKAGE-SET [OPTIONS]\n"
"\nOptions:\n"
"  --randomize            perform downloads in random order\n"
"  --exclude-name=REGEXP  exclude packages whose name matches REGEXP\n"
"  --quiet, -q            less output\n"
"  --cache=DIR, -C        path to the cache (default: ~/.cache/symboldb)\n"
"  --ignore-download-errors   process repositories with download errors\n"
"  --no-net, -N           disable most network access\n"
"  --verbose, -v          more verbose output\n\n",
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
      show_stale_cached_rpms,
      show_soname_conflicts,
      expire,
      run_example,
    } type;
  };
  namespace options {
    typedef enum {
      undefined = 2000,
      exclude_name,
      ignore_download_errors,
      randomize,
    } type;
  }
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
      {"show-stale-cached-rpms", no_argument, 0,
       command::show_stale_cached_rpms},
      {"show-soname-conflicts", required_argument, 0,
       command::show_soname_conflicts},
      {"expire", no_argument, 0, command::expire},
      {"run-example", no_argument, 0, command::run_example},
      {"exclude-name", required_argument, 0, options::exclude_name},
      {"randomize", no_argument, 0, options::randomize},
      {"cache", required_argument, 0, 'C'},
      {"no-net", no_argument, 0, 'N'},
      {"ignore-download-errors", no_argument, 0,
       options::ignore_download_errors},
      {"verbose", no_argument, 0, 'v'},
      {"quiet", no_argument, 0, 'q'},
      {0, 0, 0, 0}
    };
    int ch;
    int index;
    while ((ch = getopt_long(argc, argv, "NC:qv",
			     long_options, &index)) != -1) {
      switch (ch) {
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
      case command::show_stale_cached_rpms:
      case command::expire:
      case command::run_example:
	cmd = static_cast<command::type>(ch);
	break;
      case options::exclude_name:
	opt.add_exclude_name(optarg);
	break;
      case options::randomize:
	opt.randomize = true;
	break;
      case options::ignore_download_errors:
	opt.ignore_download_errors = true;
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
    case command::run_example:
      if (argc == optind) {
	usage(argv[0]);
      }
      break;
    case command::create_schema:
    case command::show_soname_conflicts:
    case command::expire:
      if (argc != optind) {
	usage(argv[0]);
      }
      break;
    case command::undefined:
    case command::create_set:
    case command::update_set:
    case command::update_set_from_repo:
    case command::show_stale_cached_rpms:
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
      return symboldb_download_repo(opt, db, argv + optind, false);
    case command::load_repo:
    case command::update_set_from_repo:
      return symboldb_download_repo(opt, db, argv + optind, true);
    case command::show_repomd:
      return do_show_repomd(opt, db, argv[optind]);
    case command::show_primary:
      return do_show_primary(opt, db, argv[optind]);
    case command::show_source_packages:
      return symboldb_show_source_packages(opt, argv + optind);
    case command::show_stale_cached_rpms:
      return do_show_stale_cached_rpms(opt, db);
    case command::show_soname_conflicts:
      return do_show_soname_conflicts(opt, db);
    case command::expire:
      expire(opt, db);
      return 0;
    case command::run_example:
      return do_run_example(opt, db, argv + optind);
    case command::undefined:
    default:
      abort();
    }
  } catch(curl_exception &e) {
    dump("error: ", e, stderr);
  } catch (symboldb_options::usage_error e) {
    fprintf(stderr, "error: %s\n", e.what());
  } catch (pg_exception &e) {
    fprintf(stderr, "error: from PostgreSQL:\n");
    dump("error: ", e, stderr);
  }
  return 1;
}

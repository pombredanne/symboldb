/*
 * Copyright (C) 2013 Red Hat, Inc.
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

#include <cxxll/os.hpp>
#include <cxxll/string_support.hpp>
#include <cxxll/dir_handle.hpp>
#include <cxxll/rpm_parser.hpp>
#include <cxxll/temporary_directory.hpp>
#include <cxxll/subprocess.hpp>

#include <string>

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>

#include <rpm/rpmspec.h>
#include <rpm/rpmbuild.h>

using namespace cxxll;

enum verbosity_t {
  DEFAULT_VERBOSITY, QUIET, VERBOSE
};

static std::string
find_spec_file(const char *path)
{
  if (is_directory(path)) {
    dir_handle dir(path);
    const char *spec_name = NULL;
    while (dirent *entry = dir.readdir()) {
      const char *name = entry->d_name;
      if (ends_with(name, ".spec")) {
	if (spec_name != NULL) {
	  fprintf(stderr, "error: multiple .spec files in: %s\n", path);
	  fprintf(stderr, "error:   first file: %s\n", spec_name);
	  fprintf(stderr, "error:   second file: %s\n", name);
	  exit(1);
	}
	spec_name = name;
      }
    }
    if (spec_name == NULL) {
      fprintf(stderr, "error: no .spec file found in: %s\n", path);
    }
    return realpath((std::string(path) + '/' + spec_name).c_str());
  }
  if (!path_exists(path)) {
    fprintf(stderr, "error: no such file: %s\n", path);
    exit(1);
  }
  if (is_directory(path)) {
    fprintf(stderr, "error: spec file is a directory: %s\n", path);
    exit(1);
  }
  return realpath(path);
}

static std::string
find_source_tree(const char *path)
{
  if (!is_directory(path)) {
    fprintf(stderr, "error: source tree argument is not a directory: %s\n",
	    path);
    exit(1);
  }
  return realpath(path);
}

static std::string
get_tarball_from_spec(const char *spec)
{
  rpm_parser_init();
  rpmSpec parsed = rpmSpecParse(spec, RPMSPEC_FORCE, "/var/empty");
  if (parsed == NULL) {
    fprintf(stderr, "error: could not parse spec file: %s\n", spec);
    exit(1);
  }
  rpmSpecSrcIter iter = rpmSpecSrcIterInit(parsed);
  if (iter == NULL) {
    fprintf(stderr, "error: could not extract sources from: %s\n", spec);
    exit(1);
  }
  std::string name;
  unsigned count = 0;
  while (rpmSpecSrc src = rpmSpecSrcIterNext(iter)) {
    ++count;
    name = rpmSpecSrcFilename(src, /* full */ false);
  }
  switch (count) {
  case 0:
    fprintf(stderr, "error: no source tarballs in: %s\n", spec);
    exit(1);
  case 1:
    break;
  default:
    fprintf(stderr, "error: %u source files in: %s\n", count, spec);
    exit(1);
  }
  rpmSpecSrcIterFree(iter);
  parsed = rpmSpecFree(parsed);
  rpm_parser_deinit();
  if (name.find('/') != std::string::npos) {
    fprintf(stderr, "error: %s contains tarball with '/': %s",
	    spec, name.c_str());
  }
  return name;
}

namespace {
  struct tarball_compressor {
    std::string compressed_name;
    std::string uncompressed_name;
    std::string compressor;
    explicit tarball_compressor(const std::string &tarball);
    std::string build(const temporary_directory &tempdir,
		      const std::string &source_tree);
  };

  tarball_compressor::tarball_compressor(const std::string &tarball)
  {
    compressed_name = tarball;
    uncompressed_name = tarball;
    if (ends_with(uncompressed_name, ".tar.gz")) {
      uncompressed_name.resize(uncompressed_name.size() - 3);
      compressor = "gzip";
    } else if (ends_with(uncompressed_name, ".tar.bz2")) {
      uncompressed_name.resize(uncompressed_name.size() - 4);
      compressor = "bzip2";
    } else if (ends_with(uncompressed_name, ".tar.xz")) {
      uncompressed_name.resize(uncompressed_name.size() - 3);
      compressor = "xz";
    } else {
      fprintf(stderr, "error: could not determine tarball compressor: %s\n",
	      tarball.c_str());
      exit(1);
    }

    compressor = "/bin/" + compressor;
    if (!is_executable(compressor.c_str())) {
      compressor = "/usr" + compressor;
      if (!is_executable(compressor.c_str())) {
	fprintf(stderr, "error: could not find compressor: %s\n",
		compressor.c_str());
      }
    }
  }

  std::string
  tarball_compressor::build(const temporary_directory &tempdir,
			    const std::string &source_tree)
  {
    std::string tarball_full(tempdir.path(uncompressed_name.c_str()));
    if (path_exists(tarball_full.c_str())) {
      fprintf(stderr, "error: output tarball already exists: %s\n",
	      tarball_full.c_str());
      exit(1);
    }
    {
      subprocess proc;
      proc.inherit_environ();
      proc.redirect(subprocess::in, subprocess::null);
      proc.command("/usr/bin/git");
      proc.arg(("--git-dir=" + source_tree + "/.git").c_str());
      proc.arg(("--work-tree=" + source_tree).c_str());
      proc.arg("archive");
      proc.arg("--format=tar");
      proc.arg(("--output=" + tarball_full).c_str());
      proc.arg("HEAD");
      proc.start();
      int ret = proc.wait();
      if (ret != 0) {
	fprintf(stderr, "error: \"git archive\" exited with status: %d\n", ret);
	exit(1);
      }
    }
    if (!path_exists(tarball_full.c_str())) {
      fprintf(stderr, "error: \"git archive\" did not create tarball\n");;
      exit(1);
    }

    std::string compressed_full(tempdir.path(compressed_name.c_str()));
    if (path_exists(compressed_full.c_str())) {
      fprintf(stderr, "error: output tarball already exists: %s\n",
	      compressed_full.c_str());
      exit(1);
    }
    {
      subprocess proc;
      proc.inherit_environ();
      proc.redirect(subprocess::in, subprocess::null);
      proc.command(compressor.c_str());
      proc.arg(tarball_full.c_str());
      proc.start();
      int ret = proc.wait();
      if (ret != 0) {
	fprintf(stderr, "error: %s exited with status: %d\n",
		compressor.c_str(), ret);
	exit(1);
      }
    }
    if (!path_exists(compressed_full.c_str())) {
      fprintf(stderr, "error: %s did not create compressed tarball\n",
	      compressor.c_str());;
      exit(1);
    }

    return compressed_full;
  }
}

static void
usage(const char *progname, const char *error = NULL)
{
  if (error) {
    fprintf(stderr, "error: %s\n", error);
  }
  fprintf(stderr, "Usage: %1$s [OPTIONS] [SPEC [DIRECTORY]]\n\n"
"Builds a SRPM from SPEC, using DIRECTORY as the contents of the tarball.\n\n"
"SPEC can be a directory, and it must contain a single *.spec file.  If SPEC\n"
"is a file, it is use directly.  If SPEC or DIRECTORY are omitted, they\n"
"default to the current directory.\n"
"\nOptions:\n"
"  --quiet, -q            less output, only print the SRPM path on success\n"
"  --verbose, -v          more verbose output\n\n",
	  progname);
  exit(2);
}

int
main(int argc, char **argv)
{
    static const struct option long_options[] = {
      {"verbose", no_argument, 0, 'v'},
      {"quiet", no_argument, 0, 'q'},
      {0, 0, 0, 0}
    };
    int ch;
    int index;
    verbosity_t verbosity = DEFAULT_VERBOSITY;
    while ((ch = getopt_long(argc, argv, "qv",
			     long_options, &index)) != -1) {
      switch (ch) {
      case 'q':
	verbosity = QUIET;
	break;
      case 'v':
	verbosity = VERBOSE;
	break;
      default:
	usage(argv[0]);
      }
    }
    const char *spec_arg;
    const char *tree_arg;
    switch (argc - optind) {
    case 0:
      spec_arg = ".";
      tree_arg = ".";
      break;
    case 1:
      spec_arg = argv[optind];
      tree_arg = ".";
      break;
    case 2:
      spec_arg = argv[optind];
      tree_arg = argv[optind + 1];
      break;
    default:
      usage(argv[0], "too many arguments");
    }
    std::string spec_file(find_spec_file(spec_arg));
    if (verbosity != QUIET) {
      fprintf(stderr, "info: spec file: %s\n", spec_file.c_str());
    }
    std::string source_tree(find_source_tree(tree_arg));
    if (verbosity != QUIET) {
      fprintf(stderr, "info: source tree: %s\n", source_tree.c_str());
    }
    std::string tarball(get_tarball_from_spec(spec_file.c_str()));
    if (verbosity != QUIET) {
      fprintf(stderr, "info: tarball: %s\n", tarball.c_str());
    }
    tarball_compressor compressed(tarball);
    if (verbosity != QUIET) {
      fprintf(stderr, "info: uncompressed: %s, with %s\n",
	      compressed.uncompressed_name.c_str(),
	      compressed.compressor.c_str());
    }
    {
      temporary_directory tempdir;
      std::string full_tarball_path(compressed.build(tempdir, source_tree));
      if (verbosity != QUIET) {
	fprintf(stderr, "info: built tarball: %s\n",
		full_tarball_path.c_str());
      }
    }
    return 0;
}

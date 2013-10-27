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

    return 0;
}

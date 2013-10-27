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
#include <cxxll/rpmtd_wrapper.hpp>

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

namespace {
  struct spec_info {
    std::string tarball;
    std::string name;
    std::string version;
    std::string srpm;

    explicit spec_info(const std::string &path);
  };

  spec_info::spec_info(const std::string &path)
  {
    rpm_parser_init();
    rpmSpec spec = rpmSpecParse(path.c_str(), RPMSPEC_FORCE, "/var/empty");
    if (spec == NULL) {
      fprintf(stderr, "error: could not parse spec file: %s\n", path.c_str());
      exit(1);
    }

    // Determine tarball.
    rpmSpecSrcIter iter = rpmSpecSrcIterInit(spec);
    if (iter == NULL) {
      fprintf(stderr, "error: could not extract sources from: %s\n", path.c_str());
      exit(1);
    }
    unsigned count = 0;
    while (rpmSpecSrc src = rpmSpecSrcIterNext(iter)) {
      ++count;
      tarball = rpmSpecSrcFilename(src, /* full */ false);
    }
    switch (count) {
    case 0:
      fprintf(stderr, "error: no source tarballs in: %s\n", path.c_str());
      exit(1);
    case 1:
      break;
    default:
      fprintf(stderr, "error: %u source files in: %s\n", count, path.c_str());
      exit(1);
    }
    if (tarball.find('/') != std::string::npos) {
      fprintf(stderr, "error: %s contains tarball with '/': %s",
	      path.c_str(), tarball.c_str());
    }
    rpmSpecSrcIterFree(iter);

    Header header = rpmSpecSourceHeader(spec);
    rpmtd_wrapper td;
    const char *str;

    // Determine SRPM name.
    if (!headerGet(header, RPMTAG_NVR, td.raw, HEADERGET_ALLOC | HEADERGET_EXT)
	|| (str = rpmtdGetString(td.raw)) == NULL) {
      fprintf(stderr, "error: could not obtain NVR from: %s\n", path.c_str());
      exit(1);
    }
    srpm = str;
    srpm += ".src.rpm";

    if (!headerGet(header, RPMTAG_NAME, td.raw, 0)
	|| (str = rpmtdGetString(td.raw)) == NULL) {
      fprintf(stderr, "error: could not obtain NAME from: %s\n", path.c_str());
      exit(1);
    }
    name = str;

    if (!headerGet(header, RPMTAG_VERSION, td.raw, 0)
	|| (str = rpmtdGetString(td.raw)) == NULL) {
      fprintf(stderr, "error: could not obtain VERSION from: %s\n", path.c_str());
      exit(1);
    }
    version = str;

    spec = rpmSpecFree(spec);
    rpm_parser_deinit();
  }

  struct tarball_compressor {
    std::string prefix;
    std::string compressed_name;
    std::string uncompressed_name;
    std::string compressor;
    explicit tarball_compressor(const std::string &tarball);
    std::string build(const temporary_directory &tempdir,
		      const std::string &source_tree,
		      const spec_info &spec);
  };

  tarball_compressor::tarball_compressor(const std::string &tarball)
  {
    prefix = tarball;
    compressed_name = tarball;
    uncompressed_name = tarball;
    size_t suffix_len = 2;
    if (ends_with(uncompressed_name, ".tar.gz")) {
      compressor = "gzip";
    } else if (ends_with(uncompressed_name, ".tar.bz2")) {
      suffix_len = 3;
      compressor = "bzip2";
    } else if (ends_with(uncompressed_name, ".tar.xz")) {
      compressor = "xz";
    } else {
      fprintf(stderr, "error: could not determine tarball compressor: %s\n",
	      tarball.c_str());
      exit(1);
    }
    prefix.resize(prefix.size() - suffix_len - 5);
    uncompressed_name.resize(uncompressed_name.size() - suffix_len - 1);

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
			    const std::string &source_tree,
			    const spec_info &spec)
  {
    std::string tarball_full(tempdir.path(uncompressed_name.c_str()));
    if (path_exists(tarball_full.c_str())) {
      fprintf(stderr, "error: output tarball already exists: %s\n",
	      tarball_full.c_str());
      exit(1);
    }
    {
      std::string prefixopt("--prefix=");
      {
	std::string expected(spec.name);
	expected += '-';
	expected += spec.version;
	if (prefix != expected) {
	  fprintf(stderr, "warning: unexpected name of tarball: %s\n",
		  prefix.c_str());
	  fprintf(stderr, "warning:   expected: %s\n", expected.c_str());
	}
	// This seems to allow more packages to actually build.
	prefixopt += expected;
	prefixopt += '/';
      }

      subprocess proc;
      proc.inherit_environ();
      proc.redirect(subprocess::in, subprocess::null);
      proc.command("/usr/bin/git");
      proc.arg(("--git-dir=" + source_tree + "/.git").c_str());
      proc.arg(("--work-tree=" + source_tree).c_str());
      proc.arg("archive");
      proc.arg("--format=tar");
      proc.arg(("--output=" + tarball_full).c_str());
      proc.arg(prefixopt.c_str());
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
cp_or_mv(const char *cpmv, const char *from, const char *to)
{
  subprocess proc;
  proc.command(cpmv);
  proc.arg("--").arg(from).arg(to);
  proc.start();
  int ret = proc.wait();
  if (ret != 0) {
    fprintf(stderr, "error: %s \"%s\" \"%s\" failed", cpmv, from, to);
    exit(1);
  }
}

static void
move_file(const char *from, const char *to)
{
  cp_or_mv("/bin/mv", from, to);
}

static std::string
run_rpmbuild(const temporary_directory &tempdir,
	     const std::string &spec_path,
	     const spec_info &spec)
{
  subprocess proc;
  proc.command("/usr/bin/rpmbuild");
  proc.arg("--buildroot=/var/empty");
  proc.arg(("-D _topdir " + tempdir.path()).c_str());
  proc.arg(("-D _sourcedir " + tempdir.path()).c_str());
  proc.arg(("-D _srcrpmdir " + tempdir.path()).c_str());
  proc.arg("-bs");
  proc.arg(spec_path.c_str());
  proc.start();
  int ret = proc.wait();
  if (ret != 0) {
    fprintf(stderr, "error: rpmbuild failed with exit status: %d\n", ret);
    exit(1);
  }
  std::string result(tempdir.path(spec.srpm.c_str()));
  if (!path_exists(result.c_str())) {
    fprintf(stderr, "error: rpmbuild failed to produce SRPM file: %s\n",
	    result.c_str());
    exit(1);
  }
  return result;
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
"  --output=PATH, -o PATH   output location of the SRPM file\n"
"  --quiet, -q              less output, only print the SRPM path on success\n"
"  --verbose, -v            more verbose output\n\n",
	  progname);
  exit(2);
}

int
main(int argc, char **argv)
{
    static const struct option long_options[] = {
      {"output", required_argument, 0, 'o'},
      {"verbose", no_argument, 0, 'v'},
      {"quiet", no_argument, 0, 'q'},
      {0, 0, 0, 0}
    };
    int ch;
    int index;
    verbosity_t verbosity = DEFAULT_VERBOSITY;
    std::string output_path = "..";
    while ((ch = getopt_long(argc, argv, "o:qv",
			     long_options, &index)) != -1) {
      switch (ch) {
      case 'q':
	verbosity = QUIET;
	break;
      case 'v':
	verbosity = VERBOSE;
	break;
      case 'o':
	output_path = optarg;
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

    spec_info spec(spec_file);
    if (is_directory(output_path.c_str())) {
      output_path = realpath(output_path.c_str());
      output_path += '/';
      output_path += spec.srpm;
    }
    if (verbosity != QUIET) {
      fprintf(stderr, "info: output SRPM: %s\n", output_path.c_str());
      fprintf(stderr, "info: tarball: %s\n", spec.tarball.c_str());
    }
    tarball_compressor compressed(spec.tarball);
    if (verbosity != QUIET) {
      fprintf(stderr, "info: uncompressed: %s, with %s\n",
	      compressed.uncompressed_name.c_str(),
	      compressed.compressor.c_str());
    }
    {
      temporary_directory tempdir;
      std::string full_tarball_path
	(compressed.build(tempdir, source_tree, spec));
      if (verbosity != QUIET) {
	fprintf(stderr, "info: built tarball: %s\n",
		full_tarball_path.c_str());
      }
      std::string srpm(run_rpmbuild(tempdir, spec_file, spec));
      move_file(srpm.c_str(), output_path.c_str());
      if (verbosity != QUIET) {
	fprintf(stderr, "info: created SRPM file: %s\n", output_path.c_str());
      }
    }
    return 0;
}

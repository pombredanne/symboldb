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

// pgtestshell creates a temporary database and opens a shell with a
// PGHOST environment variable pointing to it.

#include "pg_testdb.hpp"
#include "subprocess.hpp"
#include "string_support.hpp"

#include <stdlib.h>
#include <unistd.h>

int
main(int argc, char **argv)
{
  pg_testdb db;
  subprocess proc;
  if (argc <= 1) {
    const char *shell = getenv("SHELL");
    if (shell == NULL) {
      shell = "/bin/sh";
    }
    proc.command(shell);
  } else {
    proc.command(argv[1]);
    char **p = argv + 2;
    while (*p) {
      proc.arg(*p);
      ++p;
    }
  }

  // Do not inherit PG* environment variables because they could lead
  // astray PostgreSQL operations in the subprocess.
  for (char **p = environ; *p; ++p) {
    std::string key_val(*p);
    size_t sep = key_val.find('=');
    if (!starts_with(key_val, "PG") && sep != std::string::npos) {
      std::string key(key_val.begin(), key_val.begin() + sep);
      std::string val(key_val.begin() + sep + 1, key_val.end());
      proc.env(key.c_str(), val.c_str());
    }
  }

  proc.env("PGHOST", db.directory().c_str());
  proc.start();
  int ret = proc.wait();
  if (ret < 0) {
    fprintf(stderr, "error: subprocess killed by signal %d\n", -ret);
    return 255;
  }
  return ret;
}

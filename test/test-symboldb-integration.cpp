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

#include <cxxll/subprocess.hpp>
#include <cxxll/fd_handle.hpp>
#include <cxxll/os.hpp>
#include <cxxll/fd_source.hpp>
#include <cxxll/fd_sink.hpp>
#include <cxxll/source_sink.hpp>

#include <unistd.h>

#include "test.hpp"

using namespace cxxll;

static void
test()
{
  fd_handle tmp;
  tmp.mkstemp((temporary_directory_path() + "/symboldb-test-").c_str());

  subprocess proc;
  proc.command("build/pgtestshell");
  proc.arg("/bin/bash").arg("test/test-symboldb-integration.sh");
  proc.inherit_environ();
  proc.env("symboldb", "build/symboldb");
  proc.redirect(subprocess::in, subprocess::null);
  proc.redirect_to(subprocess::out, tmp.get());
  proc.redirect_to(subprocess::err, tmp.get());
  proc.start();
  int ret = proc.wait();
  if (ret != 0) {
    COMPARE_NUMBER(ret, 0);
    lseek64(tmp.get(), 0, SEEK_SET);
    fd_source src(tmp.get());
    fd_sink dst(2);		// stderr
    copy_source_to_sink(src, dst);
  }
}

static test_register t("symboldb-integration", test);



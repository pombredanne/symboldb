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

#include "subprocess.hpp"
#include "fd_source.hpp"
#include "fd_sink.hpp"
#include "string_sink.hpp"
#include "source_sink.hpp"
#include "fd_handle.hpp"

#include <signal.h>
#include <unistd.h>

#include "test.hpp"

static void
test()
{
  {
    subprocess proc;
  }
  {
    subprocess proc;
    proc.command("/bin/true");
    proc.start();
    CHECK(proc.wait() == 0);
  }
  {
    subprocess proc;
    proc.command("/bin/false");
    proc.start();
    CHECK(proc.wait() == 1);
  }
  {
    subprocess proc;
    proc.command("/bin/sh").arg("-c").arg("exit 3").start();
    CHECK(proc.wait() == 3);
  }
  {
    subprocess proc;
    proc.command("/bin/cat");
    proc.start();
    proc.kill();
    CHECK(proc.wait() == -SIGKILL);
  }
  {
    subprocess proc;
    proc.command("/bin/cat");
    proc.start();
    proc.kill(SIGTERM);
    CHECK(proc.wait() == -SIGTERM);
  }
  {
    subprocess proc;
    proc.command("/bin/echo").arg("Hello, world!");
    proc.redirect(subprocess::out, subprocess::pipe);
    proc.start();
    int fd = proc.pipefd(subprocess::out);
    fd_source source(fd);
    string_sink sink;
    copy_source_to_sink(source, sink);
    CHECK(proc.wait() == 0);
    COMPARE_STRING(sink.data, "Hello, world!\n");
  }
  {
    subprocess proc;
    proc.command("/bin/sh").arg("-c").arg("echo 'Hello, world!' 1>&2");
    proc.redirect(subprocess::err, subprocess::pipe);
    proc.start();
    int fd = proc.pipefd(subprocess::err);
    fd_source source(fd);
    string_sink sink;
    copy_source_to_sink(source, sink);
    CHECK(proc.wait() == 0);
    COMPARE_STRING(sink.data, "Hello, world!\n");
  }
  {
    subprocess proc;
    proc.command("/bin/sh").arg("-c")
      .arg("echo 'Hello, out!'; echo 'Hello, err!' 1>&2");
    proc.redirect(subprocess::out, subprocess::pipe);
    proc.redirect(subprocess::err, subprocess::pipe);
    proc.start();
    // This relies on the pipe buffer.
    int fd_out = proc.pipefd(subprocess::out);
    fd_source source_out(fd_out);
    string_sink sink_out;
    copy_source_to_sink(source_out, sink_out);
    int fd_err = proc.pipefd(subprocess::err);
    fd_source source_err(fd_err);
    string_sink sink_err;
    copy_source_to_sink(source_err, sink_err);
    CHECK(proc.wait() == 0);
    COMPARE_STRING(sink_out.data, "Hello, out!\n");
    COMPARE_STRING(sink_err.data, "Hello, err!\n");
  }
  {
    subprocess proc;
    proc.command("/bin/readlink").arg("/proc/self/fd/0");
    proc.redirect(subprocess::out, subprocess::pipe);
    proc.start();
    int fd = proc.pipefd(subprocess::out);
    fd_source source(fd);
    string_sink sink;
    copy_source_to_sink(source, sink);
    CHECK(proc.wait() == 0);
    CHECK(sink.data != "/dev/null\n");
    proc.redirect(subprocess::in, subprocess::null);
    proc.start();
    sink.data.clear();
    source.raw = proc.pipefd(subprocess::out);
    copy_source_to_sink(source, sink);
    CHECK(proc.wait() == 0);
    COMPARE_STRING(sink.data, "/dev/null\n");
  }
  {
    // Test relies on pipe buffering for correctness.
    subprocess proc;
    proc.command("/usr/bin/rev");
    proc.redirect(subprocess::in, subprocess::pipe);
    proc.redirect(subprocess::out, subprocess::pipe);
    proc.start();
    fd_sink sink(proc.pipefd(subprocess::in));
    sink.write(reinterpret_cast<const unsigned char *>("abc"), 3);
    proc.closefd(subprocess::in);
    fd_source source(proc.pipefd(subprocess::out));
    string_sink strsink;
    copy_source_to_sink(source, strsink);
    CHECK(proc.wait() == 0);
    COMPARE_STRING(strsink.data, "cba\n");
  }
  {
    subprocess proc;
    proc.command("/bin/sh").arg("-c").arg("echo $HOME");
    proc.redirect(subprocess::out, subprocess::pipe);
    proc.start();
    int fd = proc.pipefd(subprocess::out);
    fd_source source(fd);
    string_sink sink;
    copy_source_to_sink(source, sink);
    CHECK(proc.wait() == 0);
    COMPARE_STRING(sink.data, "\n");

    proc.env("HOME", "/does-not-exist");
    proc.start();
    source.raw = proc.pipefd(subprocess::out);
    sink.data.clear();
    copy_source_to_sink(source, sink);
    CHECK(proc.wait() == 0);
    COMPARE_STRING(sink.data, "/does-not-exist\n");
  }
  {
    fd_handle tempfile;
    std::string path(tempfile.mkstemp("/tmp/test-subprocess-"));
    {
      subprocess proc;
      proc.command("/bin/sh").arg("-c").arg("echo abc");
      proc.redirect_to(subprocess::out, tempfile.get());
      proc.start();
      CHECK(proc.wait() == 0);
      tempfile.open_read_only(path.c_str());
      fd_source source(tempfile.get());
      string_sink sink;
      copy_source_to_sink(source, sink);
      COMPARE_STRING(sink.data, "abc\n");
    }

    {
      subprocess proc;
      proc.command("/bin/cat").arg("--").arg(path.c_str());
      tempfile.open_read_only(path.c_str());
      proc.redirect_to(subprocess::in, tempfile.get());
      tempfile.close();
      proc.redirect(subprocess::out, subprocess::pipe);
      proc.start();
      int fd = proc.pipefd(subprocess::out);
      fd_source source(fd);
      string_sink sink;
      copy_source_to_sink(source, sink);
      CHECK(proc.wait() == 0);
      COMPARE_STRING(sink.data, "abc\n");
    }

    CHECK(unlink(path.c_str()) == 0);
  }
  {
    fd_handle tempfile;
    std::string path(tempfile.mkstemp("/tmp/test-subprocess-"));
    subprocess proc;
    proc.command("/bin/sh").arg("-c").arg("echo abc 1>&2");
    proc.redirect_to(subprocess::err, tempfile.get());
    proc.start();
    CHECK(proc.wait() == 0);
    tempfile.open_read_only(path.c_str());
    fd_source source(tempfile.get());
    string_sink sink;
    copy_source_to_sink(source, sink);
    COMPARE_STRING(sink.data, "abc\n");
    CHECK(unlink(path.c_str()) == 0);
  }
}

static test_register t("subprocess", test);

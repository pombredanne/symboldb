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

#include <cxxll/dir_handle.hpp>
#include <cxxll/fd_handle.hpp>
#include <cxxll/os.hpp>

#include <set>
#include <string>

#include "test.hpp"

using namespace cxxll;

static void
test()
{
  {
    int fd;
    {
      fd_handle h;
      h.open_read_only("/dev/null");
      fd = h.get();
    }
    {
      dir_handle h("/dev");
      CHECK(h.dirfd() == fd);
    }
    {
      fd_handle h;
      h.open_read_only("/dev/null");
      CHECK(h.get() == fd);
    }
  }

  std::string tempdir = make_temporary_directory("/tmp/test-dir_handle-");
  try {
    {
      dir_handle h(tempdir.c_str());
      CHECK(h.readdir() == NULL);
    }

    {
      std::set<std::string> files;
      dir_handle h(tempdir.c_str());
      while (dirent *e = h.readdir_all()) {
	files.insert(e->d_name);
      }
      CHECK(files.size() == 2);
      COMPARE_STRING(*files.begin(), ".");
      COMPARE_STRING(*++files.begin(), "..");
    }

    make_directory_hierarchy((tempdir + "/subdir").c_str(), 0700);
    {
      std::set<std::string> files;
      dir_handle h(tempdir.c_str());
      dirent *e = h.readdir();
      CHECK(e != NULL);
      COMPARE_STRING(e->d_name, "subdir");
      CHECK(h.readdir() == NULL);
    }

    {
      std::set<std::string> files;
      dir_handle h(tempdir.c_str());
      while (dirent *e = h.readdir_all()) {
	files.insert(e->d_name);
      }
      CHECK(files.size() == 3);
      COMPARE_STRING(*files.begin(), ".");
      COMPARE_STRING(*++files.begin(), "..");
      COMPARE_STRING(*++(++files .begin()), "subdir");
    }

  } catch (...) {
    remove_directory_tree(tempdir.c_str());
    throw;
  }
  remove_directory_tree(tempdir.c_str());
}

static test_register t("dir_handle", test);

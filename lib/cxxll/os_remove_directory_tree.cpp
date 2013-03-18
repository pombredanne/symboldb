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
#include <cxxll/os_exception.hpp>
#include <cxxll/fd_handle.hpp>

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include <string>
#include <vector>

namespace {
  class DIR_stack {
  public:
    ~DIR_stack()
    {
      while (!stack.empty()) {
	pop();
      }
    }

    bool empty() const
    {
      return stack.empty();
    }

    struct entry {
      DIR *raw;
      std::string name;

      entry(DIR *p, const char *n)
	: raw(p), name(n)
      {
      }
    };

    entry &top()
    {
      return stack.back();
    }

    void push(DIR *p, const char *name)
    {
      try {
	stack.push_back(entry(p, name));
      } catch (...) {
	if (p != NULL) {
	  closedir(p);
	}
	throw;
      }
    }

    void pop()
    {
      DIR *p = stack.back().raw;
      if (p != NULL) {
	closedir(p);
      }
      stack.pop_back();
    }

  private:
    std::vector<entry> stack;
  };
}

void
remove_directory_tree(const char *root)
{
  fd_handle fd;
  fd.open(root, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
  DIR_stack stack;
  stack.push(fdopendir(fd.get()), "");
  if (stack.top().raw == NULL) {
    throw os_exception().fd(fd.get()).path(root).function(fdopendir).defaults();
  }
  fd.release(); // ownership was taken by DIR_stack
  while (!stack.empty()) {
    errno = 0;
    DIR *dirp = stack.top().raw;
    dirent *e = readdir(dirp);
    if (e == NULL) {
      if (errno != 0) {
	throw os_exception().fd(fd.get()).function(readdir).defaults();
      }
      std::string name;
      std::swap(name, stack.top().name);
      stack.pop();
      if (!name.empty()) {
	assert(!stack.empty());
	int dirpfd = dirfd(stack.top().raw);
	int ret = unlinkat(dirpfd, name.c_str(), AT_REMOVEDIR);
	if (ret != 0) {
	  throw os_exception().fd(dirpfd).path2(name.c_str())
	    .function(unlinkat).defaults();
	}
      }
      continue;
    }
    if (e->d_name[0] == '.'
	&& (e->d_name[1] == '\0'
	    || (e->d_name[1] == '.' && e->d_name[2] == '\0'))) {
      continue;
    }
    int dirpfd = dirfd(dirp);
    int ret = unlinkat(dirpfd, e->d_name, 0);
    if (ret != 0) {
      if (errno == EISDIR) {
	// Descend into subdirectory.
	fd.openat(dirpfd, e->d_name, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
	stack.push(fdopendir(fd.get()), e->d_name);
	if (stack.top().raw == NULL) {
	  throw os_exception().fd(dirpfd).path2(e->d_name)
	    .function(fdopendir).defaults();
	}
	fd.release(); // ownership was taken by DIR_stack
	continue;
      }
      throw os_exception().fd(dirpfd).path2(e->d_name).function(unlinkat)
	.defaults();
    }
  }
  if (rmdir(root) != 0) {
    throw os_exception().path(root).function(rmdir);
  }
}

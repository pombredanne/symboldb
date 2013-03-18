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

// Magic for obtaining the name of a function.

#include <cxxll/os_exception.hpp>

#include <algorithm>
#include <sstream>

#include <dlfcn.h>
#include <stdio.h>

using namespace cxxll;

namespace {
  void
  relative_address(std::ostream &s, void *base, void *p)
  {
    if (base != p) {
      if (base < p) {
	s << '+';
      } else {
	s << '-';
	std::swap(base, p);
      }
      s << (static_cast<char *>(p) - static_cast<char *>(base));
    }
  }
}

os_exception &
os_exception::function(void *addr)
{
  function_ = addr;
  Dl_info info;
  int ret = dladdr(addr, &info);
  if (ret == 0 || info.dli_fname == NULL) {
    // This leaks information about the address space layout.
    char buf[128];
    snprintf(buf, sizeof(buf), "[%p]", addr);
    return set(function_name_, buf);
  }
  try {
    std::ostringstream desc;
    desc << std::hex;
    if (info.dli_sname) {
      desc << info.dli_sname;
      relative_address(desc, info.dli_saddr, addr);
    }
    desc << '[' << info.dli_fname;
    relative_address(desc, info.dli_fbase, addr);
    desc << ']';
    function_name_ = desc.str();
  } catch (std::bad_alloc &) {
    bad_alloc_ = true;
  }
  return *this;
}

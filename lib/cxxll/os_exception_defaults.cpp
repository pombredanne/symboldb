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

// Separate translation unit because of operating-system-dependent
// magic.

#include <cxxll/os.hpp>
#include <cxxll/os_exception.hpp>

#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace cxxll;

namespace {
  std::string
  path_from_fd(int fd)
  {
    char buf[128];
    snprintf(buf, sizeof(buf), "/proc/self/fd/%d", fd);
    return readlink(buf);
  }
}

os_exception &
os_exception::defaults()
{
  if (path_.empty() && fd_ >= 0) {  
    try {
      path_ = path_from_fd(fd_);
    } catch (...) {
    }
  }
  if (!offset_set_ && fd_ >= 0) {
    off64_t ret = lseek64(fd_, 0, SEEK_CUR);
    if (ret != static_cast<off64_t>(-1)) {
      offset_ = ret;
      offset_set_ = true;
    }
  }
  if (!length_set_ && fd_ >= 0) {
    struct stat64 st;
    if (fstat64(fd_, &st) == 0) {
      length_ = st.st_size;
      length_set_ = true;
    }
  }
  return *this;
}

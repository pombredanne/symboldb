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

#include "checksum.hpp"
#include "file_cache.hpp"
#include "fd_handle.hpp"
#include "string_support.hpp"
#include "hash.hpp"

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static const char TYPE[] = "sha256";

struct file_cache::impl {
  std::string root;
  fd_handle dirfd;
  impl(const char *path)
    : root(path)
  {
    if (root.size() > 0 && root.at(root.size() - 1) != '/') {
      root += '/';
    }
    dirfd.raw = open(path, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
  }
};

file_cache::file_cache(const char *path)
  : impl_(new impl(path))
{
}

file_cache::~file_cache()
{
}

bool
file_cache::valid()
{
  return impl_->dirfd.raw >= 0;
}

#include <stdio.h>

bool
file_cache::lookup_path(const checksum &csum, std::string &path)
{
  if (csum.type != TYPE) {
    return false;
  }
  struct stat st;
  std::string hex(base16_encode(csum.value.begin(), csum.value.end()));
  if( fstatat(impl_->dirfd.raw, hex.c_str(), &st, AT_SYMLINK_NOFOLLOW) == 0
      && (csum.length == checksum::no_length 
	  || csum.length == static_cast<unsigned long long>(st.st_size))
      && S_ISREG(st.st_mode)) {
    path = impl_->root;
    path += hex;
    return true;
  }
  return false;
}

bool
file_cache::add(const checksum &csum, const std::vector<unsigned char> &data,
		std::string &path, std::string &error)
{
  if (csum.type != TYPE) {
    error = "unsupported hash: ";
    error += csum.type;
    return false;
  }
  if (csum.length != checksum::no_length && csum.length != data.size()) {
    error = "data length does not match checksum";
    return false;
  }
  {
    std::vector<unsigned char> digest(hash_sha256(data));
    if (digest != csum.value) {
      error = "data does not match checksum";
      return false;
    }
  }
  std::string hex(base16_encode(csum.value.begin(), csum.value.end()));
  fd_handle fd;
  fd.raw = openat(impl_->dirfd.raw, hex.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
  if (fd.raw < 0) {
    error = "could not open file";
    return false;
  }
  if (!data.empty()) {
    const unsigned char *p = &data.front();
    const unsigned char *end = p + data.size();
    while (p != end) {
      ssize_t ret = write(fd.raw, p, end - p);
      if (ret == 0 || (ret < 0 && errno == ENOSPC)) {
	error = "disk full";
	return false;
      } else if (ret < 0) {
	error = "write error";
	return false;
      }
      p += ret;
    }
  }
  if (fsync(fd.raw) != 0) {
    error = "fsync error";
    return false;
  }
  path = impl_->root;
  path += hex;
  return true;
}

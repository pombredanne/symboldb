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
#include "os.hpp"
#include "fd_sink.hpp"
#include "tee_sink.hpp"

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

struct file_cache::add_sink::add_impl {
  std::tr1::shared_ptr<file_cache::impl> cache;
  checksum csum;
  std::string hex;		// part of the file name
  std::string error;
  fd_handle handle;
  fd_sink sink;
  sha256_sink hash;
  tee_sink tee;
  unsigned long long length;

  add_impl(const std::tr1::shared_ptr<file_cache::impl> &c)
    : cache(c), handle(), sink(), hash(), tee(&sink, &hash), length(0)
  {
  }
};

file_cache::add_sink::add_sink(file_cache &c, const checksum &csum)
  : impl_(new add_impl(c.impl_))
{
  if (csum.type != TYPE) {
    impl_->error = "unsupported hash: ";
    impl_->error += csum.type;
    return;
  }
  impl_->csum = csum;
  impl_->hex = base16_encode(csum.value.begin(), csum.value.end());
  impl_->handle.raw = openat(c.impl_->dirfd.raw, impl_->hex.c_str(),
			     O_WRONLY | O_CREAT | O_TRUNC, 0666);
  if (impl_->handle.raw < 0) {
    std::string s(error_string());
    impl_->error = "could not open file ";
    impl_->error += c.impl_->root;
    impl_->error += impl_->hex.c_str();
    impl_->error += ": ";
    impl_->error += s;
    return;
  }
  impl_->sink.raw = impl_->handle.raw;
}

file_cache::add_sink::~add_sink()
{
}

void
file_cache::add_sink::write(const unsigned char *buf, size_t len)
{
  if (impl_->error.empty()) {
    try {
      impl_->tee.write(buf, len);
    } catch (std::exception e) {
      impl_->error = "writing to ";
      impl_->error += impl_->cache->root;
      impl_->error += impl_->hex.c_str();
      impl_->error += ": ";
      impl_->error += e.what();
    }
    impl_->length += len;
  }
}

bool
file_cache::add_sink::finish(std::string &path, std::string &error)
{
  if (impl_->error.empty()) {
    if (impl_->csum.length != checksum::no_length
	&& impl_->csum.length != impl_->length) {
      error = impl_->error = "data length does not match checksum";
      return false;
    }
    std::vector<unsigned char> digest;
    impl_->hash.digest(digest);
    if (digest != impl_->csum.value) {
      error = impl_->error = "data does not match checksum";
      return false;
    }

    if (fsync(impl_->handle.raw) != 0) {
      std::string s(error_string());
      error = "fsync error: ";
      error += s;
      impl_->error = error;
      return false;
    }

    path = impl_->cache->root;
    path += impl_->hex;
    return true;
  } else {
    error = impl_->error;
    return false;
  }
}


bool
file_cache::add(const checksum &csum, const std::vector<unsigned char> &data,
		std::string &path, std::string &error)
{
  add_sink sink(*this, csum);
  if (!data.empty()) {
    sink.write(&data.front(), data.size());
  }
  return sink.finish(path, error);
}

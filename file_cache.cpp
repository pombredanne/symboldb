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
#include "base16.hpp"
#include "hash.hpp"
#include "os.hpp"
#include "fd_sink.hpp"
#include "tee_sink.hpp"
#include "os_exception.hpp"
#include "dir_handle.hpp"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

struct file_cache::impl {
  std::string root;
  fd_handle dirfd;
  impl(const char *path)
    : root(path)
  {
    if (root.size() > 0 && root.at(root.size() - 1) != '/') {
      root += '/';
    }
    dirfd.open(path, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
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
file_cache::lookup_path(const checksum &csum, std::string &path)
{
  if (csum.type != "sha" && csum.type != "sha256") {
    return false;
  }
  struct stat64 st;
  std::string hex(base16_encode(csum.value.begin(), csum.value.end()));
  if( fstatat64(impl_->dirfd.get(), hex.c_str(), &st, AT_SYMLINK_NOFOLLOW) == 0
      && (csum.length == checksum::no_length
	  || csum.length == static_cast<unsigned long long>(st.st_size))
      && S_ISREG(st.st_mode)) {
    path = impl_->root;
    path += hex;
    return true;
  }
  return false;
}

void
file_cache::digests(std::vector<std::vector<unsigned char> > &digests)
{
  int fd_copy = ::dup(impl_->dirfd.get());
  if (fd_copy < 0) {
    throw os_exception().fd(impl_->dirfd.get()).function(dup).defaults();
  }
  dir_handle dir(fd_copy);
  std::vector<unsigned char> decoded;
  while (dirent *e = dir.readdir()) {
    decoded.clear();
    try {
      base16_decode(e->d_name, e->d_name + strlen(e->d_name),
		    std::back_inserter(decoded));
    } catch (base16_decode_exception &e) {
      continue;
    }
    digests.push_back(decoded);
  }
}

struct file_cache::add_sink::add_impl {
  std::tr1::shared_ptr<file_cache::impl> cache;
  checksum csum;
  std::string hex;		// part of the file name
  std::string temp_file;
  fd_handle handle;
  fd_sink sink;
  hash_sink hash;
  tee_sink tee;
  unsigned long long length;

  add_impl(const std::tr1::shared_ptr<file_cache::impl> &c,
	   hash_sink::type hash_type)
    : cache(c), handle(), sink(), hash(hash_type), tee(&sink, &hash), length(0)
  {
  }

  ~add_impl()
  {
    // Clean up the temporary file.
    if (!temp_file.empty()) {
      try {
	cache->dirfd.unlinkat(temp_file.c_str(), 0);
      } catch (...) {
	// Swallow the exception.
      }
    }
  }
};

file_cache::add_sink::add_sink(file_cache &c, const checksum &csum)
{
  hash_sink::type hash_type = hash_sink::sha256;
  if (csum.type == "sha256") {
    // already set
  } else if (csum.type == "sha") {
    hash_type = hash_sink::sha1;
  } else {
    throw unsupported_hash(csum.type);
  }

  impl_.reset(new add_impl(c.impl_, hash_type));
  impl_->csum = csum;
  impl_->hex = base16_encode(csum.value.begin(), csum.value.end());
  impl_->temp_file = impl_->hex;
  impl_->temp_file += ".tmp";
  impl_->handle.openat(c.impl_->dirfd.get(), impl_->temp_file.c_str(),
		       O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0666);
  impl_->sink.raw = impl_->handle.get();
}

file_cache::add_sink::~add_sink()
{
}

void
file_cache::add_sink::write(const unsigned char *buf, size_t len)
{
  impl_->tee.write(buf, len);
  impl_->length += len;
}

void
file_cache::add_sink::finish(std::string &path)
{
  if (impl_->csum.length != checksum::no_length
      && impl_->csum.length != impl_->length) {
    throw checksum_mismatch("length");
  }

  std::vector<unsigned char> digest;
  impl_->hash.digest(digest);
  if (digest != impl_->csum.value) {
    throw checksum_mismatch("digest");
  }
  impl_->handle.fsync();
  impl_->handle.close();
  renameat(impl_->cache->dirfd, impl_->temp_file.c_str(),
	   impl_->cache->dirfd, impl_->hex.c_str());
  impl_->temp_file.clear();	// do not delete it

  path = impl_->cache->root;
  path += impl_->hex;
}


void
file_cache::add(const checksum &csum, const std::vector<unsigned char> &data,
		std::string &path)
{
  add_sink sink(*this, csum);
  sink.write(data.data(), data.size());
  sink.finish(path);
}

// Exceptions.
//////////////////////////////////////////////////////////////////////

file_cache::exception::~exception() throw()
{
}

file_cache::unsupported_hash::unsupported_hash(const std::string &h)
  : hash_(h)
{
}

file_cache::unsupported_hash::~unsupported_hash() throw()
{
}

const char *
file_cache::unsupported_hash::what() const throw()
{
  return hash_.c_str();
}

file_cache::checksum_mismatch::checksum_mismatch(const char *k)
  : kind_(k)
{
}

file_cache::checksum_mismatch::~checksum_mismatch() throw()
{
}

const char *
file_cache::checksum_mismatch::what() const throw()
{
  return kind_;
}

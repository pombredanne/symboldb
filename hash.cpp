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

#include "hash.hpp"
#include "fd_handle.hpp"
#include "os.hpp"

#include <algorithm>
#include <cassert>
#include <stdexcept>

#include <fcntl.h>
#include <limits.h>
#include <unistd.h>

#include <pk11pub.h>

struct sha256_sink::impl {
  PK11Context *raw;

  impl()
  {
    raw = PK11_CreateDigestContext(SEC_OID_SHA256);
    if (raw == NULL) {
      throw std::runtime_error("PK11_CreateDigestContext");
    }
  }

  ~impl()
  {
    PK11_DestroyContext(raw, PR_TRUE);
  }
};

sha256_sink::sha256_sink()
  : impl_(new impl)
{
  if (PK11_DigestBegin(impl_->raw) != SECSuccess) {
    throw std::runtime_error("PK11_DigestBegin");
  }
}

sha256_sink::~sha256_sink()
{
}

void
sha256_sink::write(const unsigned char *buf, size_t len)
{
  while (len > 0) {
    size_t to_hash = std::min(len, static_cast<size_t>(INT_MAX) / 2);
    if (PK11_DigestOp(impl_->raw, buf, to_hash) != SECSuccess) {
      throw std::runtime_error("PK11_DigestOp");
    }
    buf += to_hash;
    len -= to_hash;
  }
}

void
sha256_sink::digest(std::vector<unsigned char> &d)
{
  d.resize(32);
  unsigned len = d.size();
  if (PK11_DigestFinal(impl_->raw, &d.front(), &len, d.size()) != SECSuccess) {
    throw std::runtime_error("PK11_DigestFinal");
  }
  assert(len == d.size());
}

//////////////////////////////////////////////////////////////////////

std::vector<unsigned char>
hash_sha256(const std::vector<unsigned char> &data)
{
  sha256_sink sink;
  if (!data.empty()) {
    sink.write(&data.front(), data.size());
  }
  std::vector<unsigned char> digest;
  sink.digest(digest);
  return digest;
}


bool
hash_sha256_file(const char *path, std::vector<unsigned char> &digest,
		 std::string &error)
{
  sha256_sink sink;

  fd_handle fd;
  fd.raw = open(path, O_RDONLY | O_CLOEXEC);
  if (fd.raw < 0) {
    std::string s(error_string());
    error = "could not open file ";
    error += path;
    error += ": ";
    error += s;
    return false;
  }

  unsigned char buf[8192];
  while (true) {
    ssize_t ret = read(fd.raw, buf, sizeof(buf));
    if (ret == 0) {
      break;
    }
    if (ret < 0) {
      std::string s(error_string());
      error = "could not write to file ";
      error += path;
      error += ": ";
      error += s;
      return false;
    }
    sink.write(buf, ret);
  }

  sink.digest(digest);
  return true;
}

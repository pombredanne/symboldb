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

struct hash_sink::impl {
  PK11Context *raw;
  size_t digest_length;

  impl(type t)
  {
    SECOidTag oid;
    switch (t) {
    case sha1:
      oid = SEC_OID_SHA1;
      digest_length = 20;
      break;
    case sha256:
      oid = SEC_OID_SHA256;
      digest_length = 32;
      break;
    default:
      throw std::logic_error("invalid hash_sink::type");
    }
    raw = PK11_CreateDigestContext(oid);
    if (raw == NULL) {
      throw std::runtime_error("PK11_CreateDigestContext");
    }
  }

  ~impl()
  {
    PK11_DestroyContext(raw, PR_TRUE);
  }
};

hash_sink::hash_sink(type t)
  : impl_(new impl(t))
{
  if (PK11_DigestBegin(impl_->raw) != SECSuccess) {
    throw std::runtime_error("PK11_DigestBegin");
  }
}

hash_sink::~hash_sink()
{
}

void
hash_sink::write(const unsigned char *buf, size_t len)
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
hash_sink::digest(std::vector<unsigned char> &d)
{
  d.resize(impl_->digest_length);
  unsigned len = d.size();
  if (PK11_DigestFinal(impl_->raw, d.data(), &len, d.size()) != SECSuccess) {
    throw std::runtime_error("PK11_DigestFinal");
  }
  assert(len == d.size());
}

//////////////////////////////////////////////////////////////////////

std::vector<unsigned char>
hash(hash_sink::type t, const std::vector<unsigned char> &data)
{
  hash_sink sink(t);
  sink.write(data.data(), data.size());
  std::vector<unsigned char> digest;
  sink.digest(digest);
  return digest;
}


void
hash_file(hash_sink::type t,
	       const char *path, std::vector<unsigned char> &digest)
{
  hash_sink sink(t);
  fd_handle fd;
  fd.open_read_only(path);

  unsigned char buf[8192];
  while (true) {
    size_t ret = fd.read(buf, sizeof(buf));
    if (ret == 0) {
      break;
    }
    sink.write(buf, ret);
  }
  sink.digest(digest);
}

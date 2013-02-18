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

#include <cassert>
#include <stdexcept>

#include <pk11pub.h>

namespace {
  struct PK11Context_handle {
    PK11Context *raw;
    PK11Context_handle()
    {
      raw = PK11_CreateDigestContext(SEC_OID_SHA256);
      if (raw == NULL) {
	throw std::runtime_error("PK11_CreateDigestContext");
      }
    }

    ~PK11Context_handle()
    {
      PK11_DestroyContext(raw, PR_TRUE);
    }
  };
}

std::vector<unsigned char>
hash_sha256(const std::vector<unsigned char> &data)
{
  PK11Context_handle ctx;
  if (PK11_DigestBegin(ctx.raw) != SECSuccess) {
    throw std::runtime_error("PK11_DigestBegin");
  }
  if (!data.empty()) {
    if (PK11_DigestOp(ctx.raw, &data.front(), data.size()) != SECSuccess) {
      throw std::runtime_error("PK11_DigestOp");
    }
  }
  std::vector<unsigned char> digest(32);
  unsigned len;
  if (PK11_DigestFinal(ctx.raw, &digest.front(), &len, digest.size()) != SECSuccess) {
    throw std::runtime_error("PK11_DigestFinal");
  }
  assert(len == digest.size());
  return digest;
}


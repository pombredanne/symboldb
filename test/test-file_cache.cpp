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

#include <cxxll/file_cache.hpp>
#include <cxxll/checksum.hpp>
#include <cxxll/base16.hpp>

#include <cxxll/os.hpp>
#include "test.hpp"

#include <errno.h>
#include <unistd.h>

using namespace cxxll;

static void
test()
{
  std::string tempdir(make_temporary_directory("/tmp/test-file_cache-"));
  try {
    file_cache fc(tempdir.c_str());
    static const char valid[] = "valid";
    std::vector<unsigned char> data(valid, valid + sizeof(valid));
    checksum csum;
    csum.type = hash_sink::sha256;
    static const unsigned char csum_value[32] = {
      0x54, 0x3a, 0xfb, 0x82, 0xad, 0x21, 0xc0, 0x2e, 0x05, 0xde, 0xef,
      0x9b, 0xc5, 0x53, 0x90, 0x4b, 0x2b, 0xb6, 0xae, 0x10, 0xbe, 0x33,
      0x7d, 0x0b, 0x7c, 0xd6, 0xe1, 0x50, 0x99, 0xd1, 0x1b, 0xcf
    };
    csum.value.insert(csum.value.end(),
		      csum_value, csum_value + sizeof(csum_value));
    csum.length = sizeof(valid);
    std::string path;
    fc.add(csum, data, path);
    COMPARE_STRING(path, tempdir + '/' +
		   base16_encode(csum.value.begin(), csum.value.end()));
    CHECK(access(path.c_str(), R_OK) == 0);
    std::vector<std::vector<unsigned char> > digests;
    fc.digests(digests);
    CHECK(digests.size() == 1);
    COMPARE_STRING(base16_encode(digests.front().begin(),
				 digests.front().end()),
		   base16_encode(csum.value.begin(), csum.value.end()));
    CHECK(access((path + ".tmp").c_str(), R_OK) == -1 && errno == ENOENT);
    CHECK(unlink(path.c_str()) == 0);

    csum.length = 0;
    std::string old_path;
    std::swap(path, old_path);
    path = "abc";
    try {
      fc.add(csum, data, path);
      CHECK(0 && "missing exception");
    } catch (file_cache::checksum_mismatch &e) {
      COMPARE_STRING(e.what(), "length");
    }
    COMPARE_STRING(path, "abc");
    CHECK(access(old_path.c_str(), R_OK) == -1 && errno == ENOENT);
    CHECK(access((old_path + ".tmp").c_str(), R_OK) == -1 && errno == ENOENT);

    ++csum.value.front();
    csum.length = sizeof(valid);
    try {
      fc.add(csum, data, path);
      CHECK(0 && "missing exception");
    } catch (file_cache::checksum_mismatch &e) {
      COMPARE_STRING(e.what(), "digest");
    }
    COMPARE_STRING(path, "abc");
    CHECK(access(old_path.c_str(), R_OK) == -1 && errno == ENOENT);
    CHECK(access((old_path + ".tmp").c_str(), R_OK) == -1 && errno == ENOENT);
  } catch (...) {
    remove_directory_tree(tempdir.c_str());
    throw;
  }
  remove_directory_tree(tempdir.c_str());
}

static test_register t("file_cache", test);

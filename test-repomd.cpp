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

#include "repomd.hpp"
#include "fd_handle.hpp"
#include "fd_source.hpp"
#include "rpm_package_info.hpp"
#include "base16.hpp"

#include "test.hpp"

static void
test_primary()
{
  fd_handle handle;
  handle.open_read_only("test/data/primary.xml");
  fd_source source(handle.get());
  repomd::primary primary(&source, "test/data");
  CHECK(primary.next());
  COMPARE_STRING(primary.info().name, "opensm-libs");
  COMPARE_STRING(primary.info().arch, "x86_64");
  CHECK(primary.info().epoch == 0);
  COMPARE_STRING(primary.info().version, "3.3.15");
  COMPARE_STRING(primary.info().release, "3.fc18");
  COMPARE_STRING(hash_sink::to_string(primary.checksum().type), "sha256");
  COMPARE_STRING(base16_encode(primary.checksum().value.begin(),
			       primary.checksum().value.end()),
		 "c2c85a567d1b92dd6131bd326611b162ed485f6f97583e46459b430006908d66");
  CHECK(primary.checksum().length == 62796);
  COMPARE_STRING(primary.href(),
		 "test/data/Packages/o/opensm-libs-3.3.15-3.fc18.x86_64.rpm");
  COMPARE_STRING(primary.info().source_rpm, "opensm-3.3.15-3.fc18.src.rpm");

  CHECK(primary.next());
  COMPARE_STRING(primary.info().name, "bind");
  COMPARE_STRING(primary.info().arch, "x86_64");
  CHECK(primary.info().epoch == 32);
  COMPARE_STRING(primary.info().version, "9.9.2");
  COMPARE_STRING(primary.info().release, "5.P1.fc18");
  COMPARE_STRING(hash_sink::to_string(primary.checksum().type), "sha256");
  COMPARE_STRING(base16_encode(primary.checksum().value.begin(),
			       primary.checksum().value.end()),
		 "e8914e18e2264100d40a422ba91be6f19a803a96c5a1e464a3fef2248ad1063b");
  CHECK(primary.checksum().length == 2182152);
  COMPARE_STRING(primary.href(),
		 "test/data/Packages/b/bind-9.9.2-5.P1.fc18.x86_64.rpm");
  COMPARE_STRING(primary.info().source_rpm, "bind-9.9.2-5.P1.fc18.src.rpm");

  CHECK(primary.next());
  COMPARE_STRING(primary.info().name, "oniguruma");
  COMPARE_STRING(primary.info().arch, "i686");
  CHECK(primary.info().epoch == 0);
  COMPARE_STRING(primary.info().version, "5.9.2");
  COMPARE_STRING(primary.info().release, "4.fc18");
  COMPARE_STRING(hash_sink::to_string(primary.checksum().type), "sha256");
  COMPARE_STRING(base16_encode(primary.checksum().value.begin(),
			       primary.checksum().value.end()),
		 "4e03d256c6aacc905efdb83c7bd16bd452771758d1a0a80cea647cfe0f2c6314");
  CHECK(primary.checksum().length == 133156);
  COMPARE_STRING(primary.href(),
		 "http://example.com/root/Packages/o/oniguruma-5.9.2-4.fc18.i686.rpm");
  COMPARE_STRING(primary.info().source_rpm, "oniguruma-5.9.2-4.fc18.src.rpm");

  CHECK(!primary.next());
}

static void
test()
{
  test_primary();
}

static test_register t("repomd", test);

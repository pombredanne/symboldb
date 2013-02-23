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

#include "test.hpp"

static void
test_primary()
{
  fd_handle handle;
  handle.open_read_only("test/data/primary.xml");
  fd_source source(handle.raw);
  repomd::primary primary(&source);
  CHECK(primary.next());
  COMPARE_STRING(primary.name(), "opensm-libs");
  COMPARE_STRING(primary.sourcerpm(), "opensm-3.3.15-3.fc18.src.rpm");
  CHECK(primary.next());
  COMPARE_STRING(primary.name(), "oniguruma");
  COMPARE_STRING(primary.sourcerpm(), "oniguruma-5.9.2-4.fc18.src.rpm");
  CHECK(!primary.next());
}

static void
test()
{
  test_primary();
}

static test_register t("repomd", test);

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

#include <cxxll/gunzip_source.hpp>
#include <cxxll/memory_range_source.hpp>
#include <cxxll/source_sink.hpp>
#include <cxxll/vector_sink.hpp>
#include <cxxll/string_source.hpp>

#include "test.hpp"

static void
test()
{
  // Output from: echo "some data" | gzip | xxd -i
  static const unsigned char data[] = {
    0x1f, 0x8b, 0x08, 0x00, 0x6c, 0xb7, 0x28, 0x51, 0x00, 0x03, 0x2b, 0xce,
    0xcf, 0x4d, 0x55, 0x48, 0x49, 0x2c, 0x49, 0xe4, 0x02, 0x00, 0x19, 0xf9,
    0x01, 0xc8, 0x0a, 0x00, 0x00, 0x00
  };

  {
    memory_range_source mrsource(data, sizeof(data));
    gunzip_source gzsource(&mrsource);
    vector_sink vsink;
    copy_source_to_sink(gzsource, vsink);
    COMPARE_STRING(std::string(vsink.data.begin(), vsink.data.end()),
		   "some data\n");
  }
  {
    std::string data2;
    data2.append(data, data + sizeof(data));
    data2.append(data, data + sizeof(data));
    string_source stringsrc(data2);
    gunzip_source gzsource(&stringsrc);
    vector_sink vsink;
    copy_source_to_sink(gzsource, vsink);
    COMPARE_STRING(std::string(vsink.data.begin(), vsink.data.end()),
		   "some data\nsome data\n");
  }
}

static test_register t("gunzip_source", test);


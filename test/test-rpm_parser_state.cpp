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

#include <cxxll/rpm_parser.hpp>
#include <cxxll/rpm_script.hpp>

#include "test.hpp"

using namespace cxxll;

static void
test()
{
  rpm_parser_state parser("test/data/shared-mime-info-1.1-1.fc18.x86_64.rpm");
  std::vector<rpm_script> scripts;
  parser.scripts(scripts);
  COMPARE_NUMBER(scripts.size(), 1);
  COMPARE_STRING(rpm_script::to_string(scripts.at(0).type), "postin");
  CHECK(scripts.at(0).script_present);
  COMPARE_NUMBER(scripts.at(0).prog.size(), 1);
  COMPARE_STRING(scripts.at(0).prog.at(0), "/bin/sh");
  COMPARE_STRING(scripts.at(0).script,
    "# Should fail, as it would mean a problem in the mime database\n"
    "/usr/bin/update-mime-database /usr/share/mime &> /dev/null");
}

static test_register t("rpm_parser_state", test);

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

#include <cxxll/rpm_script.hpp>
#include <cxxll/raise.hpp>

#include <stdexcept>

using namespace cxxll;

const char *
rpm_script::to_string(kind type)
{
  switch (type) {
  case pretrans:
    return "pretrans";
  case prein:
    return "prein";
  case postin:
    return "postin";
  case preun:
    return "preun";
  case postun:
    return "postun";
  case posttrans:
    return "posttrans";
  case verify:
    return "verify";
  }
  raise<std::logic_error>("invalid cxxll::rpm_script::kind value");
}

rpm_script::rpm_script(kind k)
  : type(k), script_present(false)
{
}

rpm_script::~rpm_script()
{
}

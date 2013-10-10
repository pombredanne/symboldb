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

#pragma once

#include <string>
#include <vector>

namespace cxxll {

// Class representing a single trigger, which can have multiple
// conditions.
struct rpm_trigger {
  struct condition {
    std::string name;		// from RPMTAG_TRIGGERNAME
    std::string version;	// from RPMTAG_TRIGGERVERSION
    int flags;			// from RPMTAG_TRIGGERFLAG
    condition(const char *name, const char *version, int flags);
    ~condition();
  };

  std::string script;		// from RPMTAG_TRIGGERSCRIPTS
  std::string prog;		// from RPMTAG_TRIGGERSCRIPTPROG
  std::vector<condition> conditions;

  rpm_trigger(const char *script ,const char *prog);
  ~rpm_trigger();
};

} // namespace cxxll

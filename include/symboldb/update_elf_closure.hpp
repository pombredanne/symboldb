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

#include "database.hpp"
#include <cxxll/pgconn_handle.hpp>

#include <string>
#include <vector>

struct update_elf_closure_conflicts {
  // Called to indicate that the soname NEEDED_NAME could not be
  // satisfied for NEEDING_FILE.
  virtual void missing(database::file_id needing_file,
		       const std::string &needed_name) = 0;

  // Called to indicate a conflicting for NEEDING_FILE when looking up
  // NEEDED_NAME.  The first element of CHOICES is the chosen
  // resolution.
  virtual void conflict(database::file_id needing_file,
			const std::string &needed_name,
			const std::vector<database::file_id> &choices) = 0;

  // If true, the database is not actually written to.  The default
  // implementation returns false.
  virtual bool skip_update();
};

// If CONFLICTS is not NULL, conflicts encountered are recorded there.
void update_elf_closure(cxxll::pgconn_handle &, database::package_set_id,
			update_elf_closure_conflicts *conflicts);

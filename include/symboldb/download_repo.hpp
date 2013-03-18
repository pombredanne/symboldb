/*
 * Copyright (C) 2012, 2013 Red Hat, Inc.
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

class symboldb_options;

// Downloads the repositories in ARGV.  If LOAD is true, load the RPM
// files.  If a package set is set in OPT, update that package set.
int symboldb_download_repo(const symboldb_options &opt, database &db,
			   char **argv, bool load);

// Update the package set at the end of a repository-changing
// transaction.
void finalize_package_set(const symboldb_options &opt, database &db,
			  database::package_set_id set);


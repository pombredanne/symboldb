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

#include "symboldb_options.hpp"
#include "os.hpp"

symboldb_options::symboldb_options()
  : output(standard), no_net(false)
{
}

symboldb_options::~symboldb_options()
{
}

download_options
symboldb_options::download() const
{
  download_options d;
  if (no_net) {
    d.cache_mode = download_options::only_cache;
  }
  return d;
}

std::string
symboldb_options::rpm_cache_path() const
{
  std::string path;
  if (cache_path.empty()) {
    path = home_directory();
    path += "/.cache/symboldb";
  } else {
    path = cache_path;
  }
  path += "/rpms";
  return path;
}

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

#include <symboldb/database.hpp>
#include <symboldb/options.hpp>

#include <cxxll/base16.hpp>
#include <cxxll/fd_handle.hpp>
#include <cxxll/file_cache.hpp>

#include <algorithm>
#include <cstdio>

using namespace cxxll;

void
expire(const symboldb_options &opt, database &db)
{
  if (opt.output != symboldb_options::quiet) {
    fprintf(stderr, "info: expiring URL cache\n");
  }
  db.expire_url_cache();

  if (opt.output != symboldb_options::quiet) {
    fprintf(stderr, "info: expiring unreferenced packages\n");
  }
  db.expire_packages();

  if (opt.output != symboldb_options::quiet) {
    fprintf(stderr, "info: expiring file contents\n");
  }
  db.expire_file_contents();

  if (opt.output != symboldb_options::quiet) {
    fprintf(stderr, "info: expiring java classes\n");
  }
  db.expire_java_classes();

  if (opt.output != symboldb_options::quiet) {
    fprintf(stderr, "info: expiring unused RPMs\n");
  }
  {
    typedef std::vector<std::vector<unsigned char> > digvec;
    std::unique_ptr<file_cache> fcache(opt.rpm_cache());
    digvec fcdigests;
    fcache->digests(fcdigests);
    std::sort(fcdigests.begin(), fcdigests.end());
    digvec dbdigests;
    db.referenced_package_digests(dbdigests);
    digvec result;
    std::set_difference(fcdigests.begin(), fcdigests.end(),
			dbdigests.begin(), dbdigests.end(),
			std::back_inserter(result));
    fd_handle dir;
    dir.open_directory(opt.rpm_cache_path().c_str());
    for (const std::vector<unsigned char> &dig : result) {
      dir.unlinkat(base16_encode(dig.begin(), dig.end()).c_str(), 0);
    }
  }
}

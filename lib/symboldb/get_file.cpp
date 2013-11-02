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

#include <symboldb/get_file.hpp>
#include <symboldb/database.hpp>
#include <symboldb/options.hpp>
#include <cxxll/sink.hpp>
#include <cxxll/file_cache.hpp>
#include <cxxll/rpm_parser.hpp>
#include <cxxll/checksum.hpp>
#include <cxxll/rpm_file_entry.hpp>

using namespace cxxll;

bool get_file(const symboldb_options &opt, database &db,
	      const std::vector<unsigned char> &digest, sink &target)
{
  database::files_with_digest fwd(db, digest);
  std::unique_ptr<file_cache> fcache(opt.rpm_cache());
  checksum csum;
  csum.length = checksum::no_length;
  std::string rpm_path;
  rpm_file_entry rfe;
  while (fwd.next()) {
    csum.value = fwd.rpm_digest();
    switch (csum.value.size()) {
    case 32:
      csum.type = hash_sink::sha256;
      break;
    case 20:
      csum.type = hash_sink::sha1;
      break;
    default:
      continue;
    }
    if (fcache->lookup_path(csum, rpm_path)) {
      rpm_parser rp(rpm_path.c_str());
      while (rp.read_file(rfe)) {
	typedef std::vector<rpm_file_info>::const_iterator iterator;
	const iterator end = rfe.infos.end();
	for (iterator p = rfe.infos.begin(); p != end; ++p) {
	  if (p->name == fwd.file_name()) {
	    target.write(rfe.contents);
	    return true;
	  }
	}
      }
    }
  }
  return false;
}

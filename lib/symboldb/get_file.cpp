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

using namespace cxxll;

bool get_file(const symboldb_options &opt, database &db,
	      const std::vector<unsigned char> &digest, sink &target)
{
  database::files_with_digest fwd(db, digest);
  std::tr1::shared_ptr<file_cache> fcache(opt.rpm_cache());
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
      rpm_parser_state rps(rpm_path.c_str());
      while (rps.read_file(rfe)) {
	if (rfe.info->name == fwd.file_name()) {
	  if (rfe.info->digest.length != rfe.contents.size()) {
	    // If the file is hard-linked, we might have to continue
	    // scanning.
	    uint32_t ino = rfe.info->ino;
	    do {
	      if (!rps.read_file(rfe)) {
		return false;
	      }
	    } while (rfe.info->ino != ino
		     && rfe.info->digest.length != rfe.contents.size());
	  }
	  target.write(rfe.contents.data(), rfe.contents.size());
	  return true;
	}
      }
    }
  }
  return false;
}

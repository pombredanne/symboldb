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

#include "checksum.hpp"

#include <string>

class database;
class download_options;

struct repomd {
  struct entry {
    std::string type;		// "primary", "primary_db", etc.
    bool compressed;
    ::checksum checksum;
    ::checksum open_checksum;
    std::string href;		// location of the file

    entry();
    ~entry();
  };

  std::string base_url;         // base URL for repodata/repomd.xml
  std::string revision;		// revision number (or 0 if not present)
  std::vector<entry> entries;

  repomd();
  ~repomd();

  // Parses an XML document containing the repomd element.
  // Returns true on success, false otherwise (and updates error).
  // Updates the revision and entries members.
  bool parse(const unsigned char *, size_t, std::string &error);

  // Downloads the repodata/repomd.xml file relative to the passed
  // URL.  (A slash is appended to it if it is missng.)  Parses the
  // result.  An error message is written to ERROR, and false is
  // returned on failure.
  bool acquire(const download_options &, database &,
	       const char *url, std::string &error);
};

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

#include <cxxll/checksum.hpp>
#include <cxxll/rpm_package_info.hpp>
#include <cxxll/source.hpp>

#include <string>
#include <memory>

class database;
class download_options;

struct repomd {
  struct entry {
    std::string type;		// "primary", "primary_db", etc.
    bool compressed;
    cxxll::checksum checksum;
    cxxll::checksum open_checksum;
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
  void acquire(const download_options &, database &, const char *url);

  // Source which provides access to the primary.XML file for the
  // repository.
  class primary_xml : public cxxll::source {
    struct impl;
    std::shared_ptr<impl> impl_;
    primary_xml(const primary_xml &) = delete;
    primary_xml &operator=(const primary_xml &) = delete;
  public:
    // Download the primary.xml file from the repository.  You can use
    // download_options::always_cache because usually, the file name
    // embeds a hash of the file, so if we have a matching entry in
    // the cache, we know that it has the right contents.
    primary_xml(const repomd &, const download_options &, database &);
    ~primary_xml();

    // Returns the full URL for the (compressed) primary.xml file.
    const std::string &url();

    size_t read(unsigned char *, size_t);
  };

  // Iterator over the primary.xml file.  Call next() until it returns
  // false, and examine the accessors after each call.
  class primary {
    struct impl;
    std::shared_ptr<impl> impl_;
  public:
    // SOURCE is the byte stream with the (uncompressed) XML.
    // BASE_URL is the URL relative to which non-absolute URLs are
    // interpreted.
    primary(cxxll::source *, const char *base_url);
    ~primary();

    // Advances the iterator to the next element.
    bool next();

    // Accessors for package attributes.
    const cxxll::rpm_package_info &info() const; // without hash
    const cxxll::checksum &checksum() const;

    // From <location>, Already combined with the base URL or the
    // xml:base algorithm, accordingq to the yum algorithm.
    const std::string &href() const;
  };
};

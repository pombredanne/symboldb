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

#include "repomd.hpp"
#include "expat_minidom.hpp"
#include "string_support.hpp"
#include "download.hpp"

#include <cerrno>
#include <cstdlib>

repomd::entry::entry()
  : compressed(false)
{
}

repomd::entry::~entry()
{
}

repomd::repomd()
{
}

repomd::~repomd()
{
}

bool
repomd::parse(const unsigned char *buffer, size_t length,
	      std::string &error)
{
  using namespace expat_minidom;
  const char *buf = reinterpret_cast<const char *>(buffer);
  std::tr1::shared_ptr<element> root(expat_minidom::parse(buf, length, error));
  if (!root) {
    return false;
  }
  if (root->name != "repomd") {
    error = "invalid root element";
    return false;
  }
  element *rev = root->first_child("revision");
  if (rev) {
    revision = rev->text();
  } else {
    // Older repomd.xml files lack this element.
    revision.clear();
  }
  entries.clear();
  for(std::vector<std::tr1::shared_ptr<expat_minidom::node> >::iterator
	p = root->children.begin(), end = root->children.end(); p != end; ++p) {
    element *e = dynamic_cast<element *>(p->get());
    if (e && e->name == "data") {
      element *location = e->first_child("location");
      if (!location) {
	error = "location element missing from data element";
	return false;
      }
      const std::string &href(location->attributes["href"]);

      element *csum = e->first_child("checksum");
      if (!csum) {
	error = "checksum element missing from data element";
	return false;
      }
      element *open_csum = e->first_child("open-checksum");
      element *size = e->first_child("size");
      unsigned long long nsize = checksum::no_length; // not always present
      if (size && !parse_unsigned_long_long(strip(size->text()), nsize)) {
	error = "size element malformed";
	return false;
      }
      element *open_size = e->first_child("open-size");
      unsigned long long nopen_size;
      if (open_csum) {
	if (!open_size) {
	  nopen_size = checksum::no_length;
	} else {
	  if (!parse_unsigned_long_long(strip(open_size->text()), nopen_size)) {
	    error = "open-size element malformed";
	    return false;
	  }
	}
      } else if (open_size) {
	error = "open-size element without open-checksum element";
	return false;
      }

      entries.push_back(entry());
      entry &en(entries.back());
      en.type = e->attributes["type"];
      if (en.type.empty()) {
	error = "type attribute missing from data element";
	return false;
      }
      en.compressed = open_csum != 0;
      if (!en.checksum.set_hexadecimal
	  (csum->attributes["type"].c_str(), nsize,
	   strip(csum->text()).c_str())) {
	error = "malformed checksum";
	return false;
      }
      if (en.compressed) {
	if (!en.open_checksum.set_hexadecimal
	    (open_csum->attributes["type"].c_str(), nopen_size,
	     strip(open_csum->text()).c_str())) {
	  error = "malformed checksum";
	  return false;
	}
      } else {
	en.open_checksum = en.checksum;
      }
      en.href = href;
      if (en.href.empty()) {
	error = "malformed location element";
	return false;
      }
    }
  }
  return true;
}

bool
repomd::acquire(const download_options &opt, database &db,
	       const char *url, std::string &error)
{
  std::string base_canon(url);
  if (!base_canon.empty() && base_canon.at(base_canon.size() - 1) != '/') {
    base_canon += '/';
  }
  std::string mdurl(base_canon);
  mdurl += "repodata/repomd.xml";
  std::vector<unsigned char> data;
  if (!download(opt, db, mdurl.c_str(), data, error)) {
    return false;
  }
  if (data.empty()) {
    error = "empty document";
    return false;
  }
  if (!parse(data.data(), data.size(), error)) {
    return false;
  }
  base_url.swap(base_canon);
  return true;
}

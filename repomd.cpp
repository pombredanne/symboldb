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

namespace {
  inline bool
  whitespace(char ch)
  {
    return 0 <= ch && ch <= ' ';
  }

  const char *
  non_whitespace(const char *first, const char *last)
  {
    for (; first != last; ++first) {
      if (whitespace(*first)) {
	continue;
      }
      break;
    }
    return first;
  }

  void
  strip_inplace(std::string &s)
  {
    std::string::iterator p(s.begin());
    std::string::iterator end(s.end());
    while (p != end && whitespace(*p)) {
      ++p;
    }
    s.erase(s.begin(), p);
    p = s.end();
    end = s.begin();
    while (p != end && whitespace(*p)) {
      --p;
    }
    s.erase(p, s.end());
  }

  std::string
  strip(const std::string &s)
  {
    std::string r(s);
    strip_inplace(r);
    return r;
  }
  
  // Parses an unsigned long long while skipping white space.
  bool
  parse_ull(const std::string &text, unsigned long long &value)
  {
    const char *first = text.c_str();
    const char *last = first + text.size();
    first = non_whitespace(first, last);
    errno = 0;
    char *endptr;
    unsigned long long v = strtoull(first, &endptr, 10);
    if (errno != 0) {
      return false;
    }
    if (non_whitespace(endptr, last) != last) {
      // Trailing non-whitespace.
      return false;
    }
    value = v;
    return true;
  }
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
  if (!rev) {
    error = "missing \"revision\" element";
    return false;
  }
  revision = rev->text();
  entries.clear();
  for(std::vector<std::tr1::shared_ptr<expat_minidom::node> >::iterator
	p = root->children.begin(), end = root->children.end(); p != end; ++p) {
    element *e = dynamic_cast<element *>(p->get());
    if (e && e->name == "data") {
      element *csum = e->first_child("checksum");
      if (!csum) {
	error = "checksum element missing from data element";
	return false;
      }
      element *open_csum = e->first_child("open-checksum");
      element *size = e->first_child("size");
      if (!size) {
	error = "size element missing from data element";
	return false;
      }
      unsigned long long nsize;
      if (!parse_ull(strip(size->text()), nsize)) {
	error = "size element malformed";
	return false;
      }
      element *open_size = e->first_child("open-size");
      unsigned long long nopen_size = 0;
      if (open_csum) {
	if (!open_size) {
	  error = "open-size element missing from data element";
	  return false;
	}
	if (!parse_ull(strip(open_size->text()), nopen_size)) {
	  error = "open-size element malformed";
	  return false;
	}
      } else if (open_size) {
	error = "open-size element without open-checksum element";
	return false;
      }
      element *location = e->first_child("location");
      if (!location) {
	error = "location element missing from data element";
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
      en.href = location->attributes["href"];
      if (en.href.empty()) {
	error = "malformed location element";
	return false;
      }
    }
  }
  return true;
}

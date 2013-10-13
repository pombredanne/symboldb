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

#include <cxxll/maven_url.hpp>
#include <cxxll/expat_source.hpp>
#include <cxxll/string_support.hpp>
#include <cxxll/raise.hpp>

#include <cassert>
#include <cstring>

using namespace cxxll;

cxxll::maven_url::maven_url()
  : type(other)
{
}

cxxll::maven_url::maven_url(const char *u, kind t)
  : url(u), type(t)
{
}

const char *
cxxll::maven_url::to_string(kind type)
{
  switch (type) {
#define X(e) case e: return #e
    X(other);
    X(repository);
    X(pluginRepository);
    X(snapshotRepository);
    X(distributionManagement);
    X(scm);
    X(downloadUrl);
    X(site);
    X(connection);
    X(developerConnection);
#undef X
  }
  raise<std::runtime_error>("cxxll::maven_url::to_string");
}

maven_url::kind
classify(const char *str)
{
  // "other" and "downloadUrl" are ignored in this context.
#define X(e) if (strcmp(str, #e) == 0) return maven_url::e
  X(repository);
  X(pluginRepository);
  X(snapshotRepository);
  X(distributionManagement);
  X(scm);
#undef X
  return maven_url::other;
}

static void
extract_text_contents(expat_source &source,
		      maven_url::kind type, std::vector<maven_url> &result)
{
  source.next();		// enter <url>
  if (source.state() == expat_source::TEXT) {
    std::string text(strip(source.text_and_next()));
    result.push_back(maven_url());
    maven_url &mu(result.back());
    mu.url = text;
    mu.type = type;
  }
  source.unnest();		// leave <url>
}

static bool
extract_text_contents(expat_source &source,
		      const std::vector<std::string> &tags,
		      std::vector<maven_url> &result)
{
  const char *tag = source.name_ptr();
  if (strcmp(tag, "downloadUrl") == 0) {
    extract_text_contents(source, maven_url::downloadUrl, result);
    return true;
  }
  if (strcmp(tag, "connection") == 0) {
    extract_text_contents(source, maven_url::connection, result);
    return true;
  }
  if (strcmp(tag, "developerConnection") == 0) {
    extract_text_contents(source, maven_url::developerConnection, result);
    return true;
  }
  if (strcmp(tag, "url") == 0) {
    if (!tags.empty() && tags.back() == "site") {
      extract_text_contents(source, maven_url::site, result);
      return true;
    }
    std::vector<std::string>::const_iterator p = tags.begin();
    const std::vector<std::string>::const_iterator end = tags.end();
    for (; p != end; ++p) {
      maven_url::kind type = classify(p->c_str());
      if (type != maven_url::other) {
	extract_text_contents(source, type, result);
	return true;
      }
    }
    extract_text_contents(source, maven_url::other, result);
    return true;
  }
  return false;
}

void
cxxll::maven_url::extract(expat_source &source, std::vector<maven_url> &result)
{
  try {
    if (!source.next()) {
      return;
    }
    if (source.name() != "project") {
      // Not a POM file.
      return;
    }
  } catch (expat_source::entity_declaration &) {
    // If there are entity declarations, we cannot process the
    // document.
    throw;
  } catch (expat_source::malformed &) {
    // This is not really XML after all.
    return;
  }

  std::vector<std::string> tags;
  tags.resize(1);		// top-level tag
  source.next();
  while (true) {
    switch (source.state()) {
    case expat_source::START:
      {
	if (extract_text_contents(source, tags, result)) {
	  // Already skipped to next element.
	  continue;
	} else {
	  tags.push_back(source.name());
	}
      }
      break;
    case expat_source::END:
      assert(!tags.empty());
      tags.pop_back();
      break;
    default:
      break;
    }
    if (!source.next()) {
      break;
    }
  }
  assert(tags.empty());
}

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

#include <symboldb/show_source_packages.hpp>
#include <symboldb/options.hpp>
#include <symboldb/database.hpp>
#include <symboldb/repomd.hpp>
#include <cxxll/rpm_package_info.hpp>
#include <cxxll/task.hpp>

#include <algorithm>
#include <cstdio>
#include <set>

using namespace cxxll;

namespace {
  struct entry {
    std::string url;
    std::string url2;
    std::vector<std::string> packages;
    std::string error;

    static void callback(const symboldb_options *, entry *) throw();
  };
}

void
entry::callback(const symboldb_options *opt, entry *e) throw()
{
  try {
    database db;

    repomd rp;
    rp.acquire(opt->download(), db, e->url.c_str());
    repomd::primary_xml primary_xml(rp, opt->download_always_cache(), db);
    repomd::primary primary(&primary_xml, rp.base_url.c_str());
    e->url2 = primary_xml.url();
    while (primary.next()) {
      std::string src(primary.info().source_rpm);
      size_t dash = src.rfind('-');
      if (dash != std::string::npos) {
	src.resize(dash);	// strip release, architecture
	dash = src.rfind('-');
	if (dash != std::string::npos) {
	  src.resize(dash); // strip version
	}
      }
      if (dash == std::string::npos) {
	e->error = "malformed source RPM element: ";
	e->error += primary.info().source_rpm;
	return;
      }
      e->packages.push_back(src);
    }
    std::sort(e->packages.begin(), e->packages.end());
  } catch (std::exception &err) {
    e->error = err.what();
  }
}

int
symboldb_show_source_packages(const symboldb_options &opt, char **argv)
{
  std::vector<entry> entries;
  for (; *argv; ++argv) {
    entries.push_back(entry());
    entries.back().url = *argv;
  }
  if (entries.empty()) {
    return 0;
  }

  {
    std::vector<std::shared_ptr<task> > tasks;
    for (size_t i = 0; i < entries.size(); ++i) {
      tasks.push_back(std::shared_ptr<task>
		      (new task(std::bind(&entry::callback, &opt,
					       &entries.at(i)))));
    }
    for (size_t i = 0; i < tasks.size(); ++i) {
      tasks.at(i)->wait();
    }
  }

  std::set<std::string> packages;
  for (std::vector<entry>::const_iterator
	 p = entries.begin(), end = entries.end(); p != end; ++p) {
    packages.insert(p->packages.begin(), p->packages.end());
  }
  for (std::set<std::string>::const_iterator
	 p = packages.begin(), end = packages.end();
       p != end; ++p) {
    printf("%s\n", p->c_str());
  }

  return 0;
}

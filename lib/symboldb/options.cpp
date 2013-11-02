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

#include <symboldb/options.hpp>
#include <cxxll/os.hpp>
#include <cxxll/regex_handle.hpp>
#include <cxxll/string_support.hpp>

using namespace cxxll;

symboldb_options::symboldb_options()
  : output(standard), download_threads(3),
    no_net(false), ignore_download_errors(false), randomize(false),
    transient_rpms(false)
{
}

symboldb_options::~symboldb_options()
{
}

void
symboldb_options::add_exclude_name(const char *pattern)
{
  try {
    static_cast<void>(regex_handle(pattern));
  } catch (regex_handle::error &e) {
    throw usage_error("invalid --exclude-name regexp \"" + quote(pattern)
		      + "\": " + e.what());
  }
  exclude_names_.push_back(pattern);
}

regex_handle
symboldb_options::exclude_name() const
{
  std::string regexp("^(");
  bool first = true;
  for (std::vector<std::string>::const_iterator
	 p = exclude_names_.begin(), end = exclude_names_.end();
       p != end; ++p) {
    if (first) {
      first = false;
    } else {
      regexp += '|';
    }
    regexp += '(';
    regexp += *p;
    regexp += ')';
  }
  regexp += ")$";
  return regex_handle(regexp.c_str());
}

bool
symboldb_options::exclude_name_present() const
{
  return !exclude_names_.empty();
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

download_options
symboldb_options::download_always_cache() const
{
  // The metadata URLs include hashes, so we do not have to check the
  // cache for staleness.  But --no-net overrides that.
  download_options d;
  if (no_net) {
    d.cache_mode = download_options::only_cache;
  } else {
    d.cache_mode = download_options::always_cache;
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

std::unique_ptr<file_cache>
symboldb_options::rpm_cache() const
{
  std::string fcache_path(rpm_cache_path());
  if (!make_directory_hierarchy(fcache_path.c_str(), 0700)) {
    throw usage_error("could not create cache directory: " + rpm_cache_path());
  }
  std::unique_ptr<file_cache> ptr(new file_cache(fcache_path.c_str()));
  ptr->enable_fsync(!transient_rpms);
  return std::move(ptr);
}

//////////////////////////////////////////////////////////////////////
// symboldb_options::usage_error

symboldb_options::usage_error::usage_error(const std::string &what)
  : what_(what)
{
}

symboldb_options::usage_error::~usage_error() throw()
{
}

const char *
symboldb_options::usage_error::what() const throw()
{
  return what_.c_str();
}

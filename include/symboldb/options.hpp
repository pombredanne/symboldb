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

#pragma once

#include "download.hpp"
#include <cxxll/regex_handle.hpp>

#include <stdexcept>
#include <string>
#include <tr1/memory>
#include <vector>

class file_cache;

class symboldb_options {
  std::vector<std::string> exclude_names_;
public:
  enum {
    standard, verbose, quiet
  } output;

  std::string set_name;
  std::string cache_path;
  bool no_net;

  // If true, incomplete package sets with download errors are still
  // processed.
  bool ignore_download_errors;

  // Randomize the download order.
  bool randomize;

  symboldb_options();
  ~symboldb_options();

  // Adds a regular expression to exclude_name.  Throws usage_error if
  // the expresion is invalid.
  void add_exclude_name(const char *);

  // Returns a regular expression combining all excluded names.
  regex_handle exclude_name() const;

  // Returns true if add_exclude_name has been called.
  bool exclude_name_present() const;

  download_options download() const;

  // cache_only or always_cache, depending on no_net.
  download_options download_always_cache() const;

  std::tr1::shared_ptr<file_cache> rpm_cache() const;

  std::string rpm_cache_path() const;

  class usage_error : public std::exception {
    std::string what_;
  public:
    usage_error(const std::string &);
    ~usage_error() throw();
    const char *what() const throw();
  };
};

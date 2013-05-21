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

#include <string>
#include <tr1/memory>
#include <vector>

namespace cxxll {

// Parses a blob into a ZIP file.
class zip_file {
  struct impl;
  std::tr1::shared_ptr<impl> impl_;
  zip_file(const zip_file &); // not implemented
  zip_file &operator=(const zip_file &); // not implemented
public:
  // Returns true if the vector starts with the PK\003\004 signature
  // (although this is optional).
  static bool has_signature(const std::vector<unsigned char> &); 

  // Does not take ownership of the pointer.
  explicit zip_file(const std::vector<unsigned char> *);
  ~zip_file();

  // Calls this to load the first and subsequent file entries.
  bool next();

  // Returns the name of the current entry.
  std::string name() const;

  // Loads the data for the current entry into the vector.  Only works
  // once per entry.
  void data(std::vector<unsigned char> &);
};

} // namespace cxxll


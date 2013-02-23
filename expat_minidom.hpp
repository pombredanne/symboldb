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

#include <map>
#include <vector>
#include <string>
#include <tr1/memory>

class expat_source;

namespace expat_minidom {
  struct node {
    virtual ~node();
  };

  struct text : node {
    std::string data;
    text();
    ~text();
  };

  struct element : node {
    std::string name;
    std::map<std::string, std::string> attributes;
    std::vector<std::tr1::shared_ptr<node> > children;

    element();
    ~element();

    // Returns a pointer to the first child element of that name, or
    // NULL.
    element * first_child(const char *name);

    // Returns all the text children, concatenated.
    std::string text() const;
  };

  // Loads the element at the current position of EXPAT_SOURCE.
  // EXPAT_SOURCE must be in state INIT or START.
  std::tr1::shared_ptr<element> parse(expat_source &);
};

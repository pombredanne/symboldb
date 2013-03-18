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
#include <stdexcept>
#include <string>
#include <tr1/memory>

namespace cxxll {

class source;

// Pull-based XML parser.
class expat_source {
  struct impl;
  std::tr1::shared_ptr<impl> impl_;
  expat_source(const expat_source &); // not implemented
  expat_source &operator=(const expat_source &); // not implemented
public:
  // Creates a new XML parsers which retrieves data from SOURCE.  Does
  // not take ownership of the pointer.
  expat_source(source *);
  ~expat_source();

  // Advances to the next input element.  Call this function
  // repeatedly until it returns false (which signifies the end of the
  // document).
  bool next();

  typedef enum {
    INIT,  // next() has not been called yet
    START, // start element, name()/attributes()/attribute() valid
    END,   // end element
    TEXT,  // text element, text() valid
    EOD    // end of stream
  } state_type;

  // Returns the type at the current position.  Depending on the
  // return value, name()/attributes()/attribute()/text() are valid.
  state_type state() const;

  // Returns the element name.  Only valid with state() == START.
  std::string name() const;

  // Similar to name().
  // The pointer is invalidated by a call to next().
  const char *name_ptr() const;

  // Returns the attributes.  Only valid with state() == START.
  std::map<std::string, std::string> attributes() const;

  // Similar to attributes().  Does not clear the passed-in map.
  void attributes(std::map<std::string, std::string> &) const;

  // Returns the value of a specific attribute.
  // Only valid with state() == START.
  std::string attribute(const char *name) const;

  // Returns the contents of a text node.
  // Only valid with state() == TEXT.
  std::string text() const;

  // Similar to text().
  // The pointer is invalidated by a call to next().
  const char *text_ptr() const;

  // Consumes the current sequence of TEXT nodes and returns its
  // concatenation.  Afterwards, the new position is on a non-TEXT
  // node.
  std::string text_and_next();

  // Skips over the current element (and its contents), or the current
  // sequence of TEXT nodes.
  void skip();

  // Skip until the first non-nested END is reached, and then skip
  // that as well.
  void unnest();

  // Returns a string for the state type.  Throws std::logic_error on
  // an invalid state type.
  static const char *state_string(state_type);

  // Thrown when the accessors are used in the wrong state.
  class illegal_state : public std::exception {
    std::string what_;
  public:
    illegal_state(state_type actual, state_type expected);
    ~illegal_state() throw();
    const char *what() const throw ();
  };
};

} // namespace cxxll

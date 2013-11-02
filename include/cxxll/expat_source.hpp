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
#include <memory>

namespace cxxll {

class const_stringref;
class source;

// Pull-based XML parser.
class expat_source {
  struct impl;
  std::shared_ptr<impl> impl_;
  expat_source(const expat_source &) = delete;
  expat_source &operator=(const expat_source &) = delete;
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

  // Returns the line number at the current position.  Line numbers
  // start at 1.
  unsigned line() const;

  // Returns the column number at the current position.  Colum numbers
  // start at 1.
  unsigned column() const;

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

  // Base class for exceptions.
  class exception : public std::exception {
    unsigned line_;
    unsigned column_;
  protected:
    std::string what_;
    explicit exception(const impl *); // location from position_
    explicit exception(const impl *, bool); // location from Expat
  public:
    ~exception() throw();
    unsigned line() const;
    unsigned column() const;
    const char *what() const throw ();
  };

  // Thrown if the XML is malformed.
  class malformed : public exception {
    friend class impl;
  protected:
    const char *message_;
    std::string before_;
    std::string after_;
    malformed(const impl *, const_stringref);
  public:
    ~malformed() throw();
    const char *message() const;
    const std::string &before() const;
    const std::string &after() const;
  };

  // Thrown if an entity declaration is encountered.
  class entity_declaration : public malformed {
  public:
    entity_declaration(const impl *, const_stringref);
    ~entity_declaration() throw();
  };

  // Thrown when the accessors are used in the wrong state.
  class illegal_state : public exception {
    friend class impl;
    illegal_state(const impl *, state_type expected);
  public:
    ~illegal_state() throw();
  };

  // This is used to add location information to an underlying
  // exception in a callback.  FIXME: This should use C++11 exception
  // copying functionality.
  class source_error : public exception {
    friend class impl;
    explicit source_error(const impl *);
  public:
    ~source_error() throw();
  };
};

inline unsigned
expat_source::exception::line() const
{
  return line_;
}

inline unsigned
expat_source::exception::column() const
{
  return column_;
}

inline const char *
expat_source::malformed::message() const
{
  return message_;
}

inline const std::string &
expat_source::malformed::before() const
{
  return before_;
}

inline const std::string &
expat_source::malformed::after() const
{
  return after_;
}

} // namespace cxxll

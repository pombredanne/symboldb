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
#include <vector>
#include <ostream>

// ASDL is a simple data description language which as been described
// in the technical report, "The Zephyr Abstract Syntax Description
// Language", Wang et al. (1997),
// <http://www.cs.princeton.edu/research/techreps/TR-554-97>.

namespace cxxll {
namespace asdl {

// For now, identifiers are just C++ strings.  Identifiers are either
// type IDs or constructor IDs.
typedef std::string identifier;

// Returns true if the string is a valid type or constructor
// identifier.
bool valid_identifier(const identifier &);

// Returns true if the string is alphanumeric and starts with a
// lower-case letter.
bool valid_type(const identifier &);

// Returns true if the string is alphanumeric and starts with a
// capital letter.
bool valid_constructor(const identifier &);

// Indicates whether a field has exactly one value (single), can have
// zero or one value (optional), or zero or more values (multiple_.
typedef enum {single, optional, multiple} cardinality;

// Outputs "", "?", or "*", depending on the cardinality value.
std::ostream  &operator<<(std::ostream &, cardinality);

// Fields occur directly in ASDL types as attributes, or as the
// arguments of constructors.  The name is optional (it can be any
// valid identifier); if it is empty, the field has no name.
struct field {
  identifier type;		// must be a valid type ID
  identifier name;		// can be empty
  asdl::cardinality cardinality;
  field();
  ~field();
};
std::ostream  &operator<<(std::ostream &, const field &);
std::ostream  &operator<<(std::ostream &, const std::vector<field> &);

// A constructor is part of a sum type. It contains the name and a
// list of fields (which can be empty).
struct constructor {
  identifier name;		// must be a valid constructor identifier
  std::vector<field> fields;
  constructor();
  ~constructor();
};
std::ostream  &operator<<(std::ostream &, const constructor &);
std::ostream  &operator<<(std::ostream &, const std::vector<constructor> &);

// The right-hand side of an ASDL type definition.  Types with
// constructors are called sum types.  Types without constructors are
// sometimes referred to as product types.
//
// Unlike ML dataypes, ASDL sum types can have attributes.  Logically,
// they are fields that are associated with all constructors.
struct declarator {
  std::vector<constructor> constructors;
  std::vector<field> fields;
  declarator();
  ~declarator();
};
std::ostream  &operator<<(std::ostream &, const declarator &);

// A type definition associates a type name with a type declarator.
struct definition {
  std::string name;		// must be a valid type identifier
  asdl::declarator declarator;
  definition();
  ~definition();
};
std::ostream  &operator<<(std::ostream &, const definition &);

} // namespace cxxll::asdl
} // namespace cxxll


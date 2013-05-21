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

#include <stdexcept>
#include <vector>

namespace cxxll {

class java_class {
  const std::vector<unsigned char> *buffer_;
  
  unsigned short minor_version_;
  unsigned short major_version_;
  std::vector<unsigned> constant_pool_offsets;

  unsigned short access_flags_;
  unsigned short this_class_;
  unsigned short super_class_;

  std::vector<unsigned> interface_indexes;

  std::string class_name(unsigned short index) const;
  std::string utf8_string(unsigned short index) const;

public:
  // Returns true if the vector seems to contain a Java class.
  static bool has_signature(const std::vector<unsigned char> &);

  // Parses the Java class in the vector.  Does not take ownership of
  // the pointer.
  explicit java_class(const std::vector<unsigned char> *);
  ~java_class();

  // The underlying vector with the bytecode.
  const std::vector<unsigned char> &buffer() const;

  enum {
    ACC_PUBLIC = 0x001,
    ACC_FINAL = 0x0010,
    ACC_SUPER = 0x0020,
    ACC_INTERFACE =0x0200,
    ACC_ABSTRACT = 0x0400
  };

  enum {
    CONSTANT_Class = 7,
    CONSTANT_Fieldref = 9,
    CONSTANT_Methodref = 10,
    CONSTANT_InterfaceMethodref = 11,
    CONSTANT_String = 8,
    CONSTANT_Integer = 3,
    CONSTANT_Float = 4,
    CONSTANT_Long = 5,
    CONSTANT_Double = 6,
    CONSTANT_NameAndType = 12,
    CONSTANT_Utf8 = 1
  };

  unsigned short access_flags() const;
  std::string this_class() const;
  std::string super_class() const;

  // Returns a vector containing the names of all referenced classes.
  std::vector<std::string> class_references() const;

  // Returns the number of implemented interfaces and the name of the
  // implemented interface with the specified number.
  unsigned interface_count() const;
  std::string interface(unsigned index) const;

  // Thrown if the class format is malformed.
  class exception : public std::exception {
    std::string what_;
  public:
    explicit exception(const char *);
    ~exception() throw();
    const char *what() const throw();
  };
};

inline const std::vector<unsigned char> &
java_class::buffer() const
{
  return *buffer_;
}

inline unsigned short
java_class::access_flags() const
{
  return access_flags_;
}

}

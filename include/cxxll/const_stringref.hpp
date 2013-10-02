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

#include <ostream>
#include <string>
#include <vector>

#include <cstring>

#include <cxxll/char_cast.hpp>

namespace cxxll {

// A reference to the contents of a string-like object.  String-like
// objects can be std::string objects, std::vector objects made of
// characters, C-style NUL-terminated strings, or a sequence of bytes
// in memory.
//
// Constructing a const_stringref does not itself keep the underlying
// object containing the characters alive, nor does it make a copy.
// It is the responsibility of the caller to ensure that the data is
// not destroyed prematurely, either because the data-providing object
// goes out of scope, or because it is mutated in some way.
//
// In general, as with const references such as const std::string &,
// using const_stringref in function arguments is safe even if the
// data-providing object is a temporary because the life-time of
// temporaries is extended to the enclosing full expression.  However,
// if there is any aliasing and mutation of the data-providing object,
// it is still possible to end up with the const_stringref turning
// invalid.
class const_stringref {
  const char *start_;
  size_t size_;
    __attribute__((noreturn));
  void check_index(size_t index) const;
  void check_index_equal_ok(size_t index) const;

  // Out-of-line copies to avoid bloat.  We do not leak the this
  // pointer to these functions so that optimizations are not
  // impaired.
  static void throw_bad_index(size_t size, size_t index);
  static void throw_bad_empty() __attribute__((noreturn));
  static bool less_than(const_stringref, const_stringref);
  static bool less_than_equal(const_stringref, const_stringref);
  static const_stringref substr(const_stringref, size_t pos, size_t count);
public:
  // Initializes the string reference to an empty string.
  const_stringref();

  // Creates a reference to a C-style string.  The final NUL character
  // is not included.  NULL pointers are not allowed.
  const_stringref(const char *);
  const_stringref(const unsigned char *);

  // Creates a reference to a region of memory, with an explicit
  // length.  NULL pointers are not allowed.
  const_stringref(const char *, size_t);
  const_stringref(const void *, size_t);
  const_stringref(const unsigned char *, size_t);

  // Creates a reference to the contents of a vector.
  const_stringref(const std::vector<char> &);
  const_stringref(const std::vector<unsigned char> &);

  // Creates a reference to the contents of a C++ string.
  const_stringref(const std::string &);

  // Returns a copy of the string.
  std::string str() const;

  // Members modelled after std::string

  // Returns the pointer to the first character.  Note that this is
  // not guaranteed to be a C-style NUL-terminated string.
  const char *data() const;
  
  // Returns true if there are no characters.
  bool empty() const;

  // Returns the number of characters in the string.
  size_t size() const;

  // Returns the character at the specific index.  Throws
  // bad_string_index if the index is invalid.
  char at(size_t) const;
  char operator[](size_t) const;

  // Returns the substring starting at POS, including up to COUNT
  // characters.  Throws bad_string_index if POS > size().  If there
  // are not COUNT characters, the returned string is truncated
  // silently.
  const_stringref substr(size_t pos, size_t count = -1);

  // Chops off the specified number of characters.  Throws
  // bad_string_index if there are not enough characters in the
  // string.
  const_stringref operator+(size_t);

  // Lexicographic comparison as unsigned characters.
  bool operator==(const_stringref) const;
  bool operator<(const_stringref) const;
  bool operator<=(const_stringref) const;
  bool operator>(const_stringref) const;
  bool operator>=(const_stringref) const;

  // Members modelled after C run-time library functions.

  // Returns the length of the string, stopping at the first NUL
  // character.
  size_t nlen() const;

  // Members for iterator support.

  // Chops off characters at the start of the slice.
  const_stringref &operator++();
  const_stringref operator++(int);
  const_stringref &operator+=(size_t);
};

inline void
const_stringref::check_index(size_t index) const
{
  if (index >= size_) {
    throw_bad_index(size_, index);
  }
}

inline void
const_stringref::check_index_equal_ok(size_t index) const
{
  if (index > size_) {
    throw_bad_index(size_, index);
  }
}

inline
const_stringref::const_stringref()
  : start_(""), size_(0)
{
}

inline
const_stringref::const_stringref(const char *s)
  : start_(s), size_(strlen(s))
{
}
  
inline
const_stringref::const_stringref(const unsigned char *s)
  : start_(char_cast(s)), size_(strlen(start_))
{
}

inline
const_stringref::const_stringref(const char *s, size_t len)
  : start_(s), size_(len)
{
}
  
inline
const_stringref::const_stringref(const unsigned char *s, size_t len)
  : start_(char_cast(s)), size_(len)
{
}

inline
const_stringref::const_stringref(const void *s, size_t len)
  : start_(static_cast<const char *>(s)), size_(len)
{
}

inline
const_stringref::const_stringref(const std::vector<char> &v)
{
  size_ = v.size();
  if (size_) {
    start_ = v.data();
  } else {
    start_ = "";
  }
}
	   
inline
const_stringref::const_stringref(const std::vector<unsigned char> &v)
  : start_(char_cast(v.data())), size_(v.size())
{
  size_ = v.size();
  if (size_) {
    start_ = char_cast(v.data());
  } else {
    start_ = "";
  }
}

inline
const_stringref::const_stringref(const std::string &s)
{
  size_ = s.size();
  if (size_) {
    start_ = s.data();
  } else {
    start_ = "";
  }
}

inline std::string
const_stringref::str() const
{
  return std::string(start_, size_);
}

inline const char *
const_stringref::data() const
{
  return start_;
}

inline bool
const_stringref::empty() const
{
  return size_ == 0;
}

inline size_t
const_stringref::size() const
{
  return size_;
}

inline char
const_stringref::at(size_t index) const
{
  check_index(index);
  return start_[index];
}

inline char
const_stringref::operator[](size_t index) const
{
  return at(index);
}

inline const_stringref
const_stringref::substr(size_t pos, size_t count)
{
  return substr(*this, pos, count);
}

inline const_stringref
const_stringref::operator+(size_t index)
{
  check_index_equal_ok(index);
  return const_stringref(start_ + index, size_ - index);
}

inline bool
const_stringref::operator==(const_stringref o) const
{
  return size_ == o.size_ && memcmp(start_, o.start_, size_) == 0;
}

inline bool
const_stringref::operator<(const_stringref o) const
{
  return less_than(*this, o);
}

inline bool
const_stringref::operator<=(const_stringref o) const
{
  return less_than_equal(*this, o);
}

inline bool
const_stringref::operator>(const_stringref o) const
{
  return less_than(o, *this);
}

inline bool
const_stringref::operator>=(const_stringref o) const
{
  return less_than_equal(o, *this);
}

inline size_t
const_stringref::nlen() const
{
  return strnlen(start_, size_);
}

inline const_stringref &
const_stringref::operator++()
{
  if (size_ == 0) {
    throw_bad_empty();
  }
  ++start_;
  --size_;
  return *this;
}

inline const_stringref
const_stringref::operator++(int)
{
  const_stringref old(*this);
  this->operator++();
  return old;
}

inline const_stringref &
const_stringref::operator+=(size_t index)
{
  check_index_equal_ok(index);
  start_ += index;
  size_ -= index;
  return *this;
}


// Non-member operator definitions.

inline std::ostream &
operator<<(std::ostream &os, cxxll::const_stringref s)
{
  return os.write(s.data(), s.size());
}

inline std::string &
operator+=(std::string &str, cxxll::const_stringref s)
{
  str.append(s.data(), s.size());
  return str;
}


} // namespace cxxll

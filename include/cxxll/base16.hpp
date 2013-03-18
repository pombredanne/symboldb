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

namespace cxxll {

template <class InputIterator> std::string
base16_encode(InputIterator first, InputIterator last)
{
  std::string result;
  static const char digits[16] = {
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
    'a', 'b', 'c', 'd', 'e', 'f'
  };
  for (; first != last; ++first) {
    unsigned char ch = *first & 0xffU;
    result += digits[ch >> 4];
    result += digits[ch & 0x0f];
  }
  return result;
}

class base16_decode_exception : public std::exception {
  const char *what_;
  size_t offset_;
public:
  base16_decode_exception(const char *, size_t offset);
  ~base16_decode_exception() throw();

  size_t offset() const
  {
    return offset_;
  }

  const char *what() const throw();
};

template <class InputIterator, class OutputIterator> void
base16_decode(InputIterator first, InputIterator last,
	      OutputIterator out)
{
  size_t offset = 0;
  while (first != last) {
    unsigned char ch1 = *first;
    int val1;
    if ('0' <= ch1 && ch1 <= '9') {
      val1 = ch1 - '0';
    } else if ('A' <= ch1 && ch1 <= 'F') {
      val1 = 10 + ch1 - 'A';
    } else if ('a' <= ch1 && ch1 <= 'f') {
      val1 = 10 + ch1 - 'a';
    } else {
      throw base16_decode_exception("invalid hexadecimal digit", offset);
    }
    ++first;
    if (first == last) {
      throw base16_decode_exception("odd number of digits", offset);
    }
    ++offset;
    unsigned char ch2 = *first;
    int val2;
    if ('0' <= ch2 && ch2 <= '9') {
      val2 = ch2 - '0';
    } else if ('A' <= ch2 && ch2 <= 'F') {
      val2 = 10 + ch2 - 'A';
    } else if ('a' <= ch2 && ch2 <= 'f') {
      val2 = 10 + ch2 - 'a';
    } else {
      throw base16_decode_exception("invalid hexadecimal digit", offset);
    }
    ++first;
    ++offset;
    *out = (val1 << 4) | val2;
    ++out;
  }
}

} // namespace cxxll

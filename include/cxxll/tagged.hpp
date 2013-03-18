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

namespace cxxll {

// Wrapper class to create distinct types from a single type, T.
template <class T, class Tag>
class tagged {
  T value_;
public:
  tagged()
    : value_()
  {
  }

  explicit tagged(const T &value)
    : value_(value)
  {
  }

  T &value()
  {
    return value_;
  }

  const T &value() const
  {
    return value_;
  }

  bool operator==(const tagged &o) const { return value_ == o.value_; }
  bool operator!=(const tagged &o) const { return value_ != o.value_; }
  bool operator<=(const tagged &o) const { return value_ <= o.value_; }
  bool operator>=(const tagged &o) const { return value_ >= o.value_; }
  bool operator<(const tagged &o) const { return value_ < o.value_; }
  bool operator>(const tagged &o) const { return value_ > o.value_; }

  typedef Tag tag;
};

} // namespace cxxll

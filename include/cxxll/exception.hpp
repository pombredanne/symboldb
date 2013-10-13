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

namespace cxxll {

// Out-of-line throw operation.  Equivalent to throw T() or throw
// T(msg), but the code is not expanded inline.
template <class T> void call_throw() __attribute__((noreturn));
template <class T> void call_throw(const char *msg) __attribute__((noreturn));
template <class T> void call_throw(const std::string &msg)
  __attribute__((noreturn));

template <class T>
void call_throw()
{
  throw T();
}

template <class T>
void call_throw(const char *msg)
{
  throw T(msg);
}

template <class T>
void call_throw(const std::string &msg)
{
  throw T(msg);
}

extern template void call_throw<std::bad_alloc>();
extern template void call_throw<std::runtime_error>(const char *msg);
extern template void call_throw<std::runtime_error>(const std::string &);
extern template void call_throw<std::logic_error>(const char *msg);
extern template void call_throw<std::logic_error>(const std::string &);

}

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

#include <cxxll/cond.hpp>
#include <cxxll/os_exception.hpp>

void
cxxll::cond::throw_init_error(int errcode)
{
  throw os_exception(errcode).function(pthread_cond_init).defaults();
}

void
cxxll::cond::throw_wait_error(int errcode)
{
  throw os_exception(errcode).function(pthread_cond_wait).defaults();
}

void
cxxll::cond::throw_signal_error(int errcode)
{
  throw os_exception(errcode).function(pthread_cond_signal).defaults();
}

void
cxxll::cond::throw_broadcast_error(int errcode)
{
  throw os_exception(errcode).function(pthread_cond_broadcast).defaults();
}

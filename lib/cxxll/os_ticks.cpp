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

#include <cxxll/os.hpp>
#include <cxxll/os_exception.hpp>

#include <errno.h>
#include <time.h>

namespace {
  struct timespec_init {
    struct timespec time_;
    bool use_raw_;
    timespec_init();
    clockid_t clockid() const;
  };
  
  timespec_init::timespec_init()
  {
    use_raw_ = false;
    if (clock_gettime(CLOCK_MONOTONIC_RAW, &time_) == 0) {
      use_raw_ = true;
    } else {
      if (errno == ENOSYS) {
	use_raw_ = false;
      } else {
	throw cxxll::os_exception().function(clock_gettime).defaults();
      }
    }
    if (!use_raw_) {
      if (clock_gettime(CLOCK_MONOTONIC, &time_) != 0) {
	throw cxxll::os_exception().function(clock_gettime).defaults();
      }
    }
  }

  inline clockid_t
  timespec_init::clockid() const
  {
    if (use_raw_) {
      return CLOCK_MONOTONIC_RAW;
    } else {
      return CLOCK_MONOTONIC;
    }
  }
}

double
cxxll::ticks()
{
  static timespec_init base;
  struct timespec current;
  if (clock_gettime(base.clockid(), &current) != 0) {
    throw os_exception().function(clock_gettime).defaults();
  }
  const long ns_in_sec = 1000L * 1000L * 1000L;
  time_t sec = current.tv_sec - base.time_.tv_sec;
  long ns = current.tv_nsec - base.time_.tv_nsec;
  if (ns < 0) {
    ns += ns_in_sec;
    --sec;
  }
  return sec + ns * 1e-9;
}

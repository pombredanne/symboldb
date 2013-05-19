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

#include <cxxll/mutex.hpp>

#include <pthread.h>

namespace cxxll {

// Wrapper around pthread_cond_t.
class cond {
  pthread_cond_t cond_;
  cond(const cond &); // not implemented
  cond &operator=(const cond &); // not implemented
  static void throw_init_error(int errcode) __attribute__((noreturn));
  static void throw_wait_error(int errcode) __attribute__((noreturn));
  static void throw_signal_error(int errcode) __attribute__((noreturn));
public:
  cond();
  ~cond() throw();

  // Returns a pointer to the underlying condition object.
  pthread_cond_t *get() throw();

  // Wait on a condition variable.  The mutex must be locked when
  // called.  Spurious wake-ups may happen, so the condition must be
  // retested after these functions return.
  void wait(mutex &);
  void wait(pthread_mutex_t *);

  void signal();
};

inline
cond::cond()
{
  int ret = pthread_cond_init(&cond_, NULL);
  if (ret != 0) {
    throw_init_error(ret);
  }
}

inline
cond::~cond() throw()
{
  pthread_cond_destroy(&cond_);
}

inline pthread_cond_t *
cond::get() throw()
{
  return &cond_;
}

inline void
cond::wait(mutex &mut)
{
  wait(mut.get());
}

inline void
cond::wait(pthread_mutex_t *mut)
{
  int ret = pthread_cond_wait(&cond_, mut);
  if (ret != 0) {
    throw_wait_error(ret);
  }
}

inline void
cond::signal()
{
  int ret = pthread_cond_signal(&cond_);
  if (ret != 0) {
    throw_signal_error(ret);
  }
}



} // namespace cxxll

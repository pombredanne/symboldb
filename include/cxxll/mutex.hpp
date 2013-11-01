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

#include <pthread.h>

namespace cxxll {

// Wrapper around pthread_mutex_t.
class mutex {
  pthread_mutex_t mutex_;
  mutex(const mutex &) = delete;
  mutex &operator=(const mutex &) = delete;
  static void throw_init_error(int errcode) __attribute__((noreturn));
  static void throw_lock_error(int errcode) __attribute__((noreturn));
public:
  mutex();
  ~mutex() throw();

  // Returns a pointer to the underlying mutex object.
  pthread_mutex_t * get() throw();

  // Locks and unlocks the underlying mutex object.  lock() can throw
  // os_exception.
  void lock();
  void unlock() throw();

  // Helper class for RAII-style locking.
  class locker {
    mutex &mutex_;
    locker(const locker &) = delete;
    locker &operator=(const locker &) = delete;
  public:
    locker(mutex *);
    ~locker() throw();
  };
};

inline
mutex::mutex()
{
  int ret = pthread_mutex_init(&mutex_, nullptr);
  if (ret != 0) {
    throw_init_error(ret);
  }
}

inline
mutex::~mutex() throw()
{
  pthread_mutex_destroy(&mutex_);
}

inline pthread_mutex_t *
mutex::get() throw()
{
  return &mutex_;
}

inline void
mutex::lock()
{
  int ret = pthread_mutex_lock(&mutex_);
  if (ret != 0) {
    throw_lock_error(ret);
  }
}

inline void
mutex::unlock() throw()
{
  pthread_mutex_unlock(&mutex_);
}

inline
mutex::locker::locker(mutex *mut)
  : mutex_(*mut)
{
  mutex_.lock();
}

inline
mutex::locker::~locker() throw()
{
  mutex_.unlock();
}


} // namespace cxxll

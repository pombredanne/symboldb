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

#include "task.hpp"
#include "os_exception.hpp"

#include <pthread.h>

struct task::impl {
  std::tr1::function<void () throw()> runner;
  pthread_t thread;
  bool terminated;

  impl(std::tr1::function<void () throw()> f)
    : runner(f), terminated(true)
  {
    int ret = pthread_create(&thread, NULL, &callback, this);
    if (ret != 0) {
      throw os_exception(ret).function(pthread_create);
    }
    terminated = false;
  }

  ~impl()
  {
    if (!terminated) {
      pthread_detach(thread);
    }
  }

  static void *callback(void *closure) throw()
  {
    struct impl *impl_(static_cast<impl *>(closure));
    impl_->runner();
    return NULL;
  }
};

task::task(std::tr1::function<void() throw()> f)
  : impl_(new impl(f))
{
}

task::~task()
{
}

void
task::wait()
{
  if (impl_->terminated) {
    return;
  }
  int ret = pthread_join(impl_->thread, NULL);
  impl_->terminated = true;
  if (ret != 0) {
    throw os_exception(ret).function(pthread_join);
  }
}

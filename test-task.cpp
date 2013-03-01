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
#include "test.hpp"

#include <vector>

#include <pthread.h>

static void
callback(int *i) throw()
{
  ++*i;
}

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

static void
background_test() throw()
{
  int i = 0;
  for (int j = 0; j < 100; ++j) {
    int old = i;
    task t(std::tr1::bind(&callback, &i));
    t.wait();
    if (i != old + 1) {
      pthread_mutex_lock(&lock);
      CHECK(i == old + 1);
      pthread_mutex_unlock(&lock);
    }
  }
}

static void
test()
{
  std::vector<std::tr1::shared_ptr<task> > tasks;
  for (int k = 0; k < 5; ++k) {
    tasks.push_back(std::tr1::shared_ptr<task>(new task(background_test)));
  }
  for (int k = 0; k < 5; ++k) {
    tasks.at(k)->wait();
  }
}

static test_register t("task", test);

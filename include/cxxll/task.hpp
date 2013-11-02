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

#include <tr1/functional>
#include <memory>

namespace cxxll {

// Wrapper around a pthread.
class task {
  struct impl;
  std::shared_ptr<impl> impl_;
  task(const task &) = delete;
  task &operator=(const task &) = delete;
public:
  // Runs the function in a separate thread.  Can throw os_exception.
  task(std::tr1::function<void() throw()>);

  // Detaches from the thread if it is still running.
  ~task();

  //  Wait for termination.  Can throw os_exception.
  void wait();
};

} // namespace cxxll

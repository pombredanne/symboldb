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

#include <memory>

namespace cxxll {

class subprocess {
  struct impl;
  std::shared_ptr<impl> impl_;
public:
  subprocess();
  ~subprocess();

  // The command.  If the name does not contain a slash, it is looked
  // up using PATH.  Also sets the command name (see below).
  subprocess &command(const char *);
  const char *command() const;

  // The process name passed to the child process (the argv[0] entry).
  // Usually set along with command().
  subprocess &command_name(const char *);
  const char *command_name() const;

  // Adds an argument to the argument vector.  Must be called after
  // command()/command_name().
  subprocess &arg(const char *);

  // Adds an environment variable.
  subprocess &env(const char *key, const char *value);

  // Inherents the current process environment in the child.  The
  // default is to start with an empty environment.
  subprocess &inherit_environ();

  // Standard input, output and error.
  typedef enum {
    in = 0, out = 1, err = 2
  } standard_fd;

  typedef enum {
    inherit, null, pipe
  } fd_activity;

  // Controls what happens to the standard file descriptor.  The
  // default is to inherit it.
  subprocess &redirect(standard_fd, fd_activity);

  // Redirect a standard descriptor to a an existing file descriptor.
  subprocess &redirect_to(standard_fd, int fd);

  // Returns the pipe associated with a standard file descriptor.
  // Only valid if the matching activity() function has been called.
  // Ownership rests with this subprocess object, and calling wait()
  // closes these descriptors.
  int pipefd(standard_fd) const;

  // Closes the specified descriptor (which must be a pipe).
  void closefd(standard_fd);

  // Creates the child process.
  void start();

  // Returns true if the subprocess is running.  This information can
  // be outdated.
  bool running() const;

  // Waits for the termination of the child process.  A negative
  // return code corresponds to a signal number.
  int wait();

  // Terminates the child process.
  void kill(); // uses SIGKILL
  void kill(int signal);

  // Terminates the process if it runs.
  void destroy_nothrow() throw();
};

} // namespace cxxll

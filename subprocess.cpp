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

#include "subprocess.hpp"
#include "malloc_handle.hpp"
#include "fd_handle.hpp"
#include "os_exception.hpp"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <spawn.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include <vector>
#include <stdexcept>

namespace {
  void
  free_vector(std::vector<char *> &v)
  {
    for (std::vector<char *>::iterator p = v.begin(), end = v.end();
	 p != end; ++p) {
      char *ptr = *p;
      *p = NULL;
      free(ptr);
    }
  }

  char *
  xstrdup(const char *s)
  {
    assert(s != NULL);
    char *snew = strdup(s);
    if (snew == NULL) {
      throw std::bad_alloc();
    }
    return snew;
  }

  class attr_handle {
    attr_handle(const attr_handle &); // not implemented
    attr_handle &operator=(const attr_handle &); // not implemented
  public:
    posix_spawnattr_t raw;

    attr_handle()
    {
      if (posix_spawnattr_init(&raw) != 0) {
	throw os_exception().function(posix_spawnattr_init);
      }
    }

    ~attr_handle()
    {
      posix_spawnattr_destroy(&raw);
    }
  };

  class actions_handle {
    actions_handle(const actions_handle &); // not implemented
    actions_handle &operator=(const actions_handle &); // not implemented
  public:
    posix_spawn_file_actions_t raw;

    actions_handle()
    {
      if (posix_spawn_file_actions_init(&raw) != 0) {
	throw os_exception().function(posix_spawn_file_actions_init);
      }
    }

    ~actions_handle()
    {
      posix_spawn_file_actions_destroy(&raw);
    }
  };
}

struct subprocess::impl {
  malloc_handle<char> image;
  std::vector<char *> argv;
  std::vector<char *> envv;
  fd_activity activity[3];
  fd_handle pipes[3];
  pid_t pid;

  impl()
    : pid(0)
  {
    argv.push_back(NULL);
    envv.push_back(NULL);
    activity[0] = activity[1] = activity[2] = inherit;
  }

  ~impl()
  {
    free_vector(argv);
    free_vector(envv);
  }

 void close_pipes();
};

void
subprocess::impl::close_pipes()
{
  pipes[0].close_nothrow();
  pipes[1].close_nothrow();
  pipes[2].close_nothrow();
}

subprocess::subprocess()
  : impl_(new impl)
{
}

subprocess::~subprocess()
{
  destroy_nothrow();
}

subprocess &
subprocess::command(const char *cmd)
{
  impl_->image.reset(xstrdup(cmd));
  return command_name(cmd);
}

const char *
subprocess::command() const
{
  return impl_->image.get();
}

subprocess &
subprocess::command_name(const char *cmd)
{
  char *dup = xstrdup(cmd);
  free(impl_->argv.at(0));
  impl_->argv.at(0) = dup;
  if (impl_->argv.size() == 1) {
    impl_->argv.push_back(NULL);
  }
  return *this;
}

const char *
subprocess::command_name() const
{
  const char *name = impl_->argv.at(0);
  if (name == NULL) {
    throw std::logic_error("subprocess::command_name not set");
  }
  return name;
}

subprocess &
subprocess::arg(const char *str)
{
  impl_->argv.back() = xstrdup(str);
  impl_->argv.push_back(NULL);
  return *this;
}

subprocess &
subprocess::env(const char *key, const char *value)
{
  if (strchr(key, '=') != NULL) {
    throw std::logic_error("subprocess:env key contains '='");
  }
  char *ptr;
  int ret = asprintf(&ptr, "%s=%s", key, value);
  if (ret < 0) {
    throw std::bad_alloc();
  }
  impl_->envv.back() = ptr;
  impl_->envv.push_back(NULL);
  return *this;
}

subprocess &
subprocess::redirect(standard_fd fd, fd_activity act)
{
  assert(fd == in || fd == out || fd == err);
  assert(act == inherit || act == null || act == pipe);
  impl_->activity[fd] = act;
  return *this;
}

void
subprocess::start()
{
  if (impl_->pid != 0) {
    throw std::logic_error("subprocess::start called for running process");
  }

  attr_handle attr;
  sigset_t sigs;
  sigfillset(&sigs);
  if (posix_spawnattr_setsigdefault(&attr.raw, &sigs) != 0) {
    throw os_exception().function(posix_spawnattr_setsigdefault);
  }
  sigemptyset(&sigs);
  if (posix_spawnattr_setsigmask(&attr.raw, &sigs) != 0) {
    throw os_exception().function(posix_spawnattr_setsigmask);
  }

  try {
    actions_handle actions;
    fd_handle child_pipes[3];
    for (int i = 0; i < 3; ++i) {
      switch (impl_->activity[i]) {
      case inherit:
	break;
      case null:
	{
	  int flags = i == subprocess::in ? O_RDONLY : O_WRONLY;
	  if (posix_spawn_file_actions_addopen(&actions.raw, i,
					       "/dev/null", flags, 0) != 0) {
	    throw os_exception().function(posix_spawn_file_actions_addopen)
	      .fd(i).path("/dev/null"); // no defaults
	  }
	}
	break;
      case pipe:
	{
	  int pipe_parent_child[2];
	  if (pipe2(pipe_parent_child, O_CLOEXEC) != 0) {
	    throw os_exception().function(pipe2);
	  }
	  if (i == subprocess::in) {
	    // Direction of the pipe is different.
	    std::swap(pipe_parent_child[0], pipe_parent_child[1]);
	  }
	  impl_->pipes[i].reset(pipe_parent_child[0]);
	  assert(pipe_parent_child[1] > 2);
	  child_pipes[i].reset(pipe_parent_child[1]);
	  child_pipes[i].close_on_exec(false);
	  if (posix_spawn_file_actions_adddup2(&actions.raw,
					       child_pipes[i].get(), i) != 0) {
	    throw os_exception()
	      .function(posix_spawn_file_actions_adddup2).fd(i);
	  }
	  if (posix_spawn_file_actions_addclose(&actions.raw,
						child_pipes[i].get()) != 0) {
	    throw os_exception().function(posix_spawn_file_actions_addclose)
	      .fd(child_pipes[i].get());
	  }
	}
	break;
      default:
	assert(false);
      }
    }

    int ret = posix_spawn(&impl_->pid, impl_->image.get(),
			  &actions.raw, &attr.raw,
			  impl_->argv.data(), impl_->envv.data());
    if (ret != 0) {
      throw os_exception().function(posix_spawn).path(impl_->image.get());
    }
  } catch (...) {
    impl_->close_pipes();
    throw;
  }
}

int
subprocess::pipefd(standard_fd fd) const
{
  assert(fd == in || fd == out || fd == err);
  if (impl_->pid == 0) {
    throw std::logic_error("subprocess::wait needs started process");
  }
  if (impl_->activity[fd] != pipe) {
    throw std::logic_error("subprocess: access to non-pipe descriptor");
  }
  return impl_->pipes[fd].get();
}

void
subprocess::closefd(standard_fd fd)
{
  assert(fd == in || fd == out || fd == err);
  if (impl_->pid == 0) {
    throw std::logic_error("subprocess::wait needs started process");
  }
  if (impl_->activity[fd] != pipe) {
    throw std::logic_error("subprocess: non-pipe descriptor cannot be closed");
  }
  impl_->pipes[fd].close();
}

int
subprocess::wait()
{
  if (impl_->pid == 0) {
    throw std::logic_error("subprocess::wait needs started process");
  }
  int status;
  pid_t ret = ::waitpid(impl_->pid, &status, 0);
  impl_->pid = 0;
  int err = errno;
  impl_->close_pipes();
  if (ret == -1) {
    // FIXME: pid()?
    throw os_exception(err).function(::waitpid).offset(impl_->pid)
      .path(impl_->image.get());
  }
  if (WIFEXITED(status)) {
    return WEXITSTATUS(status);
  } else if (WIFSIGNALED(status)) {
    return -WTERMSIG(status);
  } else {
    // FIXME: pid()?
    throw os_exception(0).function(::waitpid)
      .message("unknown status (see count)").offset(impl_->pid).count(status)
      .path(impl_->image.get());
  }
}

void
subprocess::kill()
{
  kill(SIGKILL);
}

void
subprocess::kill(int signo)
{
  if (impl_->pid == 0) {
    throw std::logic_error("subprocess::kill needs started process");
  }
  if (::kill(impl_->pid, signo) != 0) {
    // FIXME: pid?
    throw os_exception().function(::kill).offset(impl_->pid)
      .path(impl_->image.get());
  }
}

void
subprocess::destroy_nothrow() throw()
{
  if (impl_->pid != 0) {
    // Terminate the subprocess.
    // FIXME: we should probably log errors.
    ::kill(impl_->pid, SIGKILL);
    ::waitpid(impl_->pid, NULL, 0);
    impl_->pid = 0;
  }
}

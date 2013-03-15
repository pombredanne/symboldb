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

#include "backtrace.hpp"

#include <cxxabi.h>
#include <dlfcn.h>
#include <inttypes.h>
#include <link.h>
#include <signal.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <unwind.h>

// FIXME: The address we get from dladdr() or via dl_iterate_phdr()
// does not work with addr2line.  This could be due to prelinking.

// FIXME
#include <stdio.h>

static volatile bool handling = false;
static const char addr2line[] = "/usr/bin/addr2line";

namespace {
  void
  write_string(const char *text)
  {
    write(STDERR_FILENO, text, strlen(text));
  }

  void
  write_concat(const char *a, const char *b, const char *c)
  {
    size_t alen = strlen(a);
    size_t blen = strlen(b);
    size_t clen = strlen(c);
    size_t len = alen + blen + clen;
    char buf[512];
    if (len > sizeof(buf)) {
      write(STDERR_FILENO, a, alen);
      write(STDERR_FILENO, b, blen);
      write(STDERR_FILENO, c, clen);
    } else {
      memcpy(buf, a, alen);
      memcpy(buf + alen, b, blen);
      memcpy(buf + alen + blen, c, clen);
      write(STDERR_FILENO, buf, len);
    }
  }

  struct dl_iterate_data {
    uintptr_t target;
    uintptr_t offset;

    explicit dl_iterate_data(uintptr_t);
    static int callback(dl_phdr_info *info, size_t, void *);
  };

  dl_iterate_data::dl_iterate_data(uintptr_t ptr)
    : target(ptr), offset(0)
  {
    dl_iterate_phdr(callback, this);
  }

  int
  dl_iterate_data::callback(dl_phdr_info *info, size_t, void *userdata)
  {
    dl_iterate_data *dlid = static_cast<dl_iterate_data *>(userdata);
    uintptr_t target = dlid->target;
    for (unsigned i = 0; i < info->dlpi_phnum; ++i) {
      uintptr_t start = info->dlpi_addr + info->dlpi_phdr[i].p_vaddr;
      uintptr_t end = start + info->dlpi_phdr[i].p_memsz;
      if (start <= target && target <= end) {
	fprintf(stderr, "* match relative to %016zx in %s\n", info->dlpi_addr, info->dlpi_name);
	fprintf(stderr, "start: %016zu  end: %016zu\n", start, end);
	dlid->offset = target - start + info->dlpi_phdr[i].p_offset;
	return 1;
      }
    }
    return 0;
  }


  struct formatted_address {
    char text[17];		// 16 digits plus NUL

    void set(uintptr_t);
  };

  void
  formatted_address::set(uintptr_t uptr)
  {
    int idx = sizeof(text) - 1;
    text[idx] = '\0';
    --idx;
    while (idx >= 0) {
      int digit = uptr & 0x0f;
      if (digit <= 9) {
	digit += '0';
      } else {
	digit += 'a' - 10;
      }
      text[idx] = digit;
      uptr = uptr >> 4;
      --idx;
    }
  }

  struct closure {
    unsigned to_skip;
    unsigned remaining_to_print;
    unsigned unresolvable;
    char last_file[512];
    unsigned addrs_len;
    enum { addrs_count = 30 };
    formatted_address addrs[addrs_count];
    bool last_file_shown;
    bool addr2line_present;

    closure();
    bool push_address(struct _Unwind_Context *context);
    bool print();
  };

  closure::closure()
    : to_skip(0), remaining_to_print(30), unresolvable(0), addrs_len(0),
      last_file_shown(false)
  {
    last_file[0] = '\0';
    addr2line_present = access(addr2line, X_OK) == 0;
  }

  uintptr_t get_address(struct _Unwind_Context *context)
  {
    int decrement;
    uintptr_t uptr;
    uptr = _Unwind_GetIPInfo(context, &decrement);
    if (decrement) {
      --uptr;
    }
    return uptr;
  }

  bool
  closure::push_address(struct _Unwind_Context *context)
  {
    uintptr_t uptr = get_address(context);
    Dl_info info;
    int ret = dladdr(reinterpret_cast<void *>(uptr), &info);
    dl_iterate_data dlid(uptr);
    if (ret == 0 || info.dli_fname == NULL || dlid.offset == 0) {
      ++unresolvable;
      return true;
    }
    if (!addr2line_present || strlen(info.dli_fname) >= sizeof(last_file)) {
      if (!print()) {
	return false;
      }
      write_string(info.dli_fname);
      formatted_address addr;
      addr.set(dlid.offset);
      write_concat("+", addr.text, "\n");
      return true;
    }
    if (addrs_len == addrs_count) {
      if (!print()) {
	return false;
      }
    }
    if (strcmp(last_file, info.dli_fname) != 0) {
      if (!print()) {
	return false;
      }
      strncpy(last_file, info.dli_fname, sizeof(last_file));
      last_file_shown = false;
    }
    addrs[addrs_len].set(dlid.offset);
    fprintf(stderr, "%016zx [%016zx] -- [%s]\n", size_t(uptr), size_t(dlid.offset), addrs[addrs_len].text);
    ++addrs_len;
    return true;
  }

  bool
  closure::print()
  {
    if (!addr2line_present || addrs_len == 0 || last_file[0] == '\0') {
      return true;
    }
    if (!last_file_shown) {
      write_concat("Stack frames in ", last_file, ":\n");
      last_file_shown = true;
    }
    const char * argv[7 + addrs_count + 1] = {
      addr2line, "-a", "-f", "-i", "-p", "-e",
    };
    argv[6] = last_file;
    for (unsigned i = 0; i < addrs_len; ++i) {
      argv[7 + i] = addrs[i].text;
    }
    argv[7 + addrs_len] = NULL;
    char * envp[] = {NULL};
    int subprocess = fork();
    if (subprocess < 0) {
      write_string("Could not create subprocess.\n");
      return false;
    } else if (subprocess == 0) {
      // In the subprocess.
      execve(addr2line, const_cast<char **>(argv), envp);
      _exit(1);
    }
    int status;
    if (waitpid(subprocess, &status, 0) < 0) {
      write_string("Could not wait for subprocess termination.\n");
      return false;
    }
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
      write_string("Subprocess failed to print backtrace information.\n");
      return false;
    }
    addrs_len = 0;
    return true;
  }

  _Unwind_Reason_Code
  callback(struct _Unwind_Context *context, void *userdata)
  {
    closure *cl = static_cast<closure *>(userdata);
    if (cl->to_skip > 0) {
      --cl->to_skip;
    } else {
      if (cl->remaining_to_print == 0) {
	return _URC_END_OF_STACK;
      }
      --cl->remaining_to_print;
      if (!cl->push_address(context)) {
	cl->remaining_to_print = 0;
	return _URC_END_OF_STACK;
      }
    }
    return _URC_NO_REASON;
  }
}

static void
handler(void)
{
  if (!handling) {
    handling = true;

    std::type_info *etype = __cxxabiv1::__cxa_current_exception_type();
    if (etype) {
      write_concat
	("Program execution terminated by exception ", etype->name(), ".\n");
      try {
	throw;
      } catch (std::exception &e) {
	write_concat("Message: ", e.what(), "\n");
      } catch (...) {
      }
    } else {
      write_string("Program execution terminated.\n");
    }
  } else {
    write_string("Exception thrown in handler.\n");
  }
  closure cl;
  if (cl.addr2line_present) {
    signal(SIGCLD, SIG_DFL);
  }
  _Unwind_Backtrace (callback, &cl);
  signal(SIGABRT, SIG_DFL);
  kill(getpid(), SIGABRT);
  _exit(255);
}

void
backtrace_init()
{
  std::set_terminate(handler);
}

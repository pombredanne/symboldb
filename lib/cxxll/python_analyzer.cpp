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

#include <cxxll/python_analyzer.hpp>
#include <cxxll/os_exception.hpp>
#include <cxxll/subprocess.hpp>
#include <cxxll/fd_sink.hpp>
#include <cxxll/fd_source.hpp>
#include <cxxll/endian.hpp>

#include <algorithm>

#include <string.h>

/*
  We use the following encoding to communicate with the Python
  subprocesses:

  Numbers are encoded as 32-bit unsigned values in big endian order.

  Strings are encoded as their length (as a number), followed by their
  bytes.

  Arrays are encoded as a number, followed by a matching number of
  array elements.

  This encoding is not self-describing, it requires knowledge of the
  types in the data stream.

  To request parsing a self-contained piece of source code, the parent
  process sends it as a single string.

  The child process responds with the error message (a string,
  possibly empty), the line number of the error (a number, zero in
  case of no error), and the extracted imports (an array of strings).
*/

using namespace cxxll;

static const char PYTHON_SCRIPT[] = {
#include "python_analyzer.py.inc"
  , 0
};

namespace {
  struct interpreter {
    subprocess process_;
    struct result {
      std::string error_;
      unsigned error_line_;
      std::vector<std::string> imports_;
      std::vector<std::string> attributes_;
    } result_;
    unsigned version_;

    interpreter(const char *python_path, unsigned version);
    bool parse(const std::vector<unsigned char> &source);
    void send_string(const std::vector<unsigned char> &);
    std::string read_string(source &);
    unsigned read_number(source &);
    void read_array(source &, std::vector<std::string> &);
  };

  interpreter::interpreter(const char *python_path, unsigned version)
    : version_(version)
  {
    process_.command(python_path);
    process_.arg("-c");
    process_.arg(PYTHON_SCRIPT);
    process_.redirect(subprocess::in, subprocess::pipe);
    process_.redirect(subprocess::out, subprocess::pipe);
    // TODO: capture standard error
    process_.start();
  }

  bool
  interpreter::parse(const std::vector<unsigned char> &source)
  {
    send_string(source);
    fd_source src(process_.pipefd(subprocess::out));
    result_.error_ = read_string(src);
    result_.error_line_ = read_number(src);
    read_array(src, result_.imports_);
    read_array(src, result_.attributes_);
    return result_.error_line_ == 0;
  }

  void
  interpreter::send_string(const std::vector<unsigned char> &str)
  {
    fd_sink sink(process_.pipefd(subprocess::in));
    union {
      unsigned number;
      unsigned char buf[sizeof(unsigned)];
    } u;
    u.number = cpu_to_be_32(str.size());
    sink.write(u.buf, sizeof(u.buf));
    sink.write(str.data(), str.size());
  }

  unsigned
  interpreter::read_number(source &src)
  {
    union {
      unsigned number;
      unsigned char buf[sizeof(unsigned)];
    } u;
    read_exactly(src, u.buf, sizeof(u.buf));
    return be_to_cpu_32(u.number);
  }

  std::string
  interpreter::read_string(source &src)
  {
    size_t size = read_number(src);
    char buf[4096];
    std::string result;
    while (size > 0) {
      size_t to_read = std::min(sizeof(buf), size);
      read_exactly(src, reinterpret_cast<unsigned char *>(buf), to_read);
      result.append(buf, to_read);
      size -= to_read;
    }
    return result;
  }

  void
  interpreter::read_array(source &src, std::vector<std::string> &res)
  {
    res.clear();
    for (unsigned count = read_number(src); count > 0; --count) {
      res.push_back(read_string(src));
    }
  }
}

struct python_analyzer::impl {
  std::tr1::shared_ptr<interpreter> python2_;
  std::tr1::shared_ptr<interpreter> python3_;
  const interpreter *active_;

  impl();
  void maybe_start(std::tr1::shared_ptr<interpreter> &,
		   const char *path, unsigned version);
  bool bad_string(const std::vector<unsigned char> &);
};

python_analyzer::impl::impl()
  : active_(NULL)
{
}

void
python_analyzer::impl::maybe_start(std::tr1::shared_ptr<interpreter> &interp,
				  const char *path, unsigned version)
{
  if (!(interp && interp->process_.running())) {
    interp.reset(new interpreter(path, version));
  }
}

bool
python_analyzer::impl::bad_string(const std::vector<unsigned char> &source)
{
  std::vector<unsigned char>::const_iterator nul
    (std::find(source.begin(), source.end(), '\0'));
  if (nul != source.end()) {
    python2_->result_.error_ = "source code contains NUL character";
    python2_->result_.error_line_ = 1 + std::count(source.begin(), nul, '\n');
    python2_->result_.imports_.clear();
    python2_->result_.attributes_.clear();
    active_ = &*python2_;
    return true;
  }
  return false;
}

python_analyzer::python_analyzer()
  : impl_(new impl)
{
}

python_analyzer::~python_analyzer()
{
}

bool
python_analyzer::parse(const std::vector<unsigned char> &source)
{
  impl_->active_ = NULL;
  impl_->maybe_start(impl_->python2_, "/usr/bin/python", 2);
  if (impl_->bad_string(source)) {
    return false;
  }
  if (impl_->python2_->parse(source)) {
    impl_->active_ = &*impl_->python2_;
    return true;
  }
  impl_->maybe_start(impl_->python3_, "/usr/bin/python3", 3);
  if (impl_->python3_->parse(source)) {
    impl_->active_ = &*impl_->python3_;
    return true;
  }
  // Pick the Python version whose parse error comes later.
  if (impl_->python3_->result_.error_line_
      > impl_->python2_->result_.error_line_) {
    impl_->active_ = &*impl_->python3_;
  } else {
    impl_->active_ = &*impl_->python2_;
  }
  return false;
}

bool
python_analyzer::good() const
{
  return impl_->active_ && impl_->active_->result_.error_line_ == 0;
}

unsigned
python_analyzer::version() const
{
  if (impl_->active_ != NULL) {
    return impl_->active_->version_;
  }
  return 0;
}

const std::vector<std::string> &
python_analyzer::imports() const
{
  if (impl_->active_ != NULL) {
    return impl_->active_->result_.imports_;
  }
  static const std::vector<std::string> empty;
  return empty;
}

const std::vector<std::string> &
python_analyzer::attributes() const
{
  if (impl_->active_ != NULL) {
    return impl_->active_->result_.attributes_;
  }
  static const std::vector<std::string> empty;
  return empty;
}

const std::string &
python_analyzer::error_message() const
{
  if (impl_->active_ != NULL) {
    return impl_->active_->result_.error_;
  }
  static const std::string empty;
  return empty;
}

unsigned
python_analyzer::error_line() const
{
  if (impl_->active_ != NULL) {
    return impl_->active_->result_.error_line_;
  }
  return 0;
}

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

#include <stdexcept>
#include <string>

// Exception class mostly intended for POSIX-related errors.
class os_exception : std::exception {
  std::string message_;
  std::string path_;
  std::string path2_;
  std::string function_name_;
  mutable std::string what_;
  void *function_;
  unsigned long long offset_;
  unsigned long long length_;
  unsigned long long count_;
  int fd_;
  int error_code_;
  bool bad_alloc_;
  bool offset_set_;
  bool length_set_;
  bool count_set_;

  void init(); // set all scalars to 0.

  // No-throwing wrappers setting bad_alloc_ on std::bad_alloc.
  os_exception &set(std::string &, const char *);
  os_exception &set(std::string &, const std::string &);
public:
  // Creates a new os_exception from errno.
  os_exception();

  // Creates a new os_exception, using the indicated error code.
  // os_exception(0) can be used to suppress the initialization from
  // errno.
  explicit os_exception(int);

  ~os_exception() throw();

  // Error message.
  const std::string &message() const;
  os_exception &message(const char *);
  os_exception &message(const std::string &);

  // errno error code.
  int error_code() const;
  os_exception &error_code(int code);

  // File system path.
  const std::string &path() const;
  os_exception &path(const char *);
  os_exception &path(const std::string &);

  // Alternative file system path.
  const std::string &path2() const;
  os_exception &path2(const char *);
  os_exception &path2(const std::string &);

  // Offset.
  unsigned long long offset() const;
  os_exception &offset(unsigned long long);

  // Length (typically of a file).
  unsigned long long length() const;
  os_exception &length(unsigned long long);

  // Count (typically an in-memory buffer length).
  unsigned long long count() const;
  os_exception &count(unsigned long long);

  // File descriptor.
  int fd() const;
  os_exception &fd(int);

  // Pointer to failing function.
  const std::string &function_name() const;
  void *function() const;
  os_exception &function(void *);
  template <class T> os_exception &function(T *);

  // Set default values of fields based on other fields.
  os_exception &defaults();

  virtual const char *what() const throw();
};

inline const std::string &
os_exception::message() const
{
  return message_;
}

inline int
os_exception::error_code() const
{
  return error_code_;
}

inline os_exception &
os_exception::error_code(int code)
{
  error_code_ = code;
  return *this;
}

inline const std::string &
os_exception::path() const
{
  return path_;
}

inline const std::string &
os_exception::path2() const
{
  return path2_;
}

inline unsigned long long
os_exception::offset() const
{
  return offset_;
}

inline os_exception &
os_exception::offset(unsigned long long c)
{
  offset_ = c;
  offset_set_ = true;
  return *this;
}

inline unsigned long long
os_exception::length() const
{
  return length_;
}

inline os_exception &
os_exception::length(unsigned long long c)
{
  length_ = c;
  length_set_ = true;
  return *this;
}

inline unsigned long long
os_exception::count() const
{
  return count_;
}

inline os_exception &
os_exception::count(unsigned long long c)
{
  count_ = c;
  count_set_ = true;
  return *this;
}

inline int
os_exception::fd() const
{
  return fd_;
}

inline os_exception &
os_exception::fd(int d)
{
  fd_ = d;
  return *this;
}

template <class T> inline os_exception &
os_exception::function(T *addr)
{
  return function(reinterpret_cast<void *>(addr));
}

inline const std::string &
os_exception::function_name() const
{
  return function_name_;
}

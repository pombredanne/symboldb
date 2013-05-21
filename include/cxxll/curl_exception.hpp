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

#include <stdexcept>
#include <string>

namespace cxxll {

// Reports a libcurl error.
class curl_exception : public std::exception {
  mutable std::string what_;
  std::string message_;
  std::string original_url_;
  std::string url_;
  std::string remote_ip_;
  int status_;
  unsigned remote_port_;
  bool bad_alloc_;
public:
  explicit curl_exception(const char *message) throw();
  ~curl_exception() throw();

  const std::string &message() const throw();

  // The response code.
  int status() const throw();
  curl_exception &status(int) throw();

  // The URL at which the error occured.
  const std::string &url() const throw();
  curl_exception &url(const char *) throw();
  curl_exception &url(const std::string &) throw();

  // The URL with which the request started.
  const std::string &original_url() const throw();
  curl_exception &original_url(const char *) throw();
  curl_exception &original_url(const std::string &) throw();

  // The remote IP address and port.
  const std::string &remote_ip() const throw();
  unsigned remote_port() const throw();
  curl_exception &remote(const char *ip, unsigned port) throw();

  const char *what() const throw();
};

inline const std::string &
curl_exception::message() const throw()
{
  return message_;
}

inline int
curl_exception::status() const throw()
{
  return status_;
}

inline curl_exception &
curl_exception::status(int st) throw()
{
  status_ = st ;
  return *this;
}

inline const std::string &
curl_exception::url() const throw()
{
  return url_;
}

inline const std::string &
curl_exception::original_url() const throw()
{
  return original_url_;
}

inline const std::string &
curl_exception::remote_ip() const throw()
{
  return remote_ip_;
}

inline unsigned
curl_exception::remote_port() const throw()
{
  return remote_port_;
}

} // namespace cxxll

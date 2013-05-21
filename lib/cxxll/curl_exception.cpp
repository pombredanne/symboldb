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

#include <cxxll/curl_exception.hpp>
#include <cxxll/string_support.hpp>

#include <cstdio>

using namespace cxxll;

static const char oom_message[] = "out of memory";

curl_exception::curl_exception(const char *message) throw()
  : status_(0), remote_port_(0), bad_alloc_(false)
{
  try {
    message_ = message;
  } catch (std::bad_alloc &) {
    bad_alloc_ = true;
  }
}

curl_exception::~curl_exception() throw()
{
}

curl_exception &
curl_exception::url(const char *str) throw()
{
  try {
    url_ = str;
  } catch (std::bad_alloc &) {
    bad_alloc_ = true;
  }
  return *this;
}

curl_exception &
curl_exception::url(const std::string &str) throw()
{
  try {
    url_ = str;
  } catch (std::bad_alloc &) {
    bad_alloc_ = true;
  }
  return *this;
}

curl_exception &
curl_exception::original_url(const char *str) throw()
{
  try {
    original_url_ = str;
  } catch (std::bad_alloc &) {
    bad_alloc_ = true;
  }
  return *this;
}

curl_exception &
curl_exception::original_url(const std::string &str) throw()
{
  try {
    original_url_ = str;
  } catch (std::bad_alloc &) {
    bad_alloc_ = true;
  }
  return *this;
}

curl_exception &
curl_exception::remote(const char *ip, unsigned port) throw()
{
  try {
    remote_ip_ = ip;
    remote_port_ = port;
  } catch (std::bad_alloc &) {
    bad_alloc_ = true;
  }
  return *this;
}

const char *
curl_exception::what() const throw()
{
  if (what_.empty()) {
    if (bad_alloc_) {
      if (message_.empty()) {
	return oom_message;
      } else {
	return message_.c_str();
      }
    }
    try {
      what_ = '"';
      what_ += quote(message_);
      what_ += '"';
      if (status_ != 0) {
	char buf[32];
	snprintf(buf, sizeof(buf), " status=%d", status_);
	what_ += buf;
      }
      if (!url_.empty()) {
	what_ += " url=\"";
	what_ += quote(url_);
	what_ += '"';
      }
      if (!remote_ip_.empty()) {
	char buf[128];
	snprintf(buf, sizeof(buf), " remote=[%s]:%u",
		 remote_ip_.c_str(), remote_port_);
	what_ += buf;
      }
      if (!original_url_.empty()) {
	what_ += " original=\"";
	what_ += quote(original_url_);
	what_ += '"';
      }
    } catch (std::bad_alloc &) {
      what_.clear();
      if (message_.empty()) {
	return oom_message;
      } else {
	return message_.c_str();
      }
    }
  }
  return what_.c_str();
}

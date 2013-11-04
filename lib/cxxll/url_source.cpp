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

#include <cxxll/url_source.hpp>
#include <cxxll/raise.hpp>
#include <cxxll/curl_exception.hpp>
#include <cxxll/curl_handle.hpp>
#include <cxxll/char_cast.hpp>
#include <cxxll/os_exception.hpp>

#include <vector>

#include <assert.h>
#include <string.h>
#include <sys/select.h>
#include <unistd.h>

using namespace cxxll;

//////////////////////////////////////////////////////////////////////
// cxxll::url_source::impl

struct cxxll::url_source::impl {
  std::string url_;
  std::string effective_url_;

  curl_handle curl_;
  CURLM *multi_;

  // Used to pass down the buffer supplied to read()
  char *buffer_;
  size_t buffer_length_;
  size_t buffer_written_;

  // Keeps excess data fed from Curl.
  std::vector<unsigned char> pending_;
  size_t pending_pos_;

  long http_date_;	     // last modification date, -1 if not available
  long long file_size_;	     // file size on the server, -1 if not available

  // Indicates whether connect() has been called.
  bool connected_;

  // Indicates that no payload data has been read over the connection.
  // This means that header information is unavailable.
  bool before_data_;

  // Indicates that the write callback has been invoked (possibly with
  // zero bytes).
  bool write_callback_;

  impl(const std::string &);
  ~impl();

  // Performs connection health checks and throws curl_exception if
  // necessary.  Updates http_data_ and http_size_.
  void check();

  // Establishes the underlying connection.
  void connect();

  // Internal read function.
  size_t read(unsigned char *, size_t);
  
  static size_t write_function(char *ptr, size_t size, size_t nmemb, void *);
};

cxxll::url_source::impl::impl(const std::string &url)
  : url_(url), pending_pos_(false), connected_(false), before_data_(true)
{
  multi_ = curl_multi_init();
  if (multi_ == NULL) {
    raise<std::bad_alloc>();
  }
}

cxxll::url_source::impl::~impl()
{
  if (connected_) {
    curl_multi_remove_handle(multi_, curl_.raw);
  }
  curl_multi_cleanup(multi_);
}

inline void
cxxll::url_source::impl::check()
{
  if (!before_data_) {
    return;
  }

  char *effective_url_c = NULL;
  curl_easy_getinfo(curl_.raw, CURLINFO_EFFECTIVE_URL, &effective_url_c);
  effective_url_ = effective_url_c;

  long status = 0;
  curl_easy_getinfo(curl_.raw, CURLINFO_RESPONSE_CODE, &status);
  // A response code of 0 is used if the protocol does not support
  // response codes.
  if (status != 200 && status != 0) {
    char *primary_ip = NULL;
    curl_easy_getinfo(curl_.raw, CURLINFO_PRIMARY_IP, &primary_ip);
    long primary_port = 0;
    curl_easy_getinfo(curl_.raw, CURLINFO_PRIMARY_PORT, &primary_port);
    if (effective_url_ == url_) {
      throw curl_exception("").url(url_).status(status)
	.remote(primary_ip, primary_port);
    } else {
      throw curl_exception("").url(effective_url_)
	.original_url(url_).status(status).remote(primary_ip, primary_port);
    }
  }
  http_date_ = -1;
  curl_easy_getinfo(curl_.raw, CURLINFO_FILETIME, &http_date_);
  double size;
  curl_easy_getinfo(curl_.raw, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &size);
  file_size_ = size;
  before_data_ = false;
}

void
cxxll::url_source::impl::connect()
{
  if (connected_) {
    return;
  }

  CURL *curl = curl_.raw;
  const std::string &url(url_);
  CURLcode ret;
  ret = curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  if (ret != CURLE_OK) {
    throw curl_exception(curl_easy_strerror(ret)).url(url);
  }
  ret = curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
  if (ret != CURLE_OK) {
    throw curl_exception(curl_easy_strerror(ret)).url(url);
  }
  long protocols = CURLPROTO_HTTP | CURLPROTO_HTTPS | CURLPROTO_FTP;
  ret = curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS, protocols);
  if (ret != CURLE_OK) {
    throw curl_exception(curl_easy_strerror(ret)).url(url);
  }
  ret = curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  if (ret != CURLE_OK) {
    throw curl_exception(curl_easy_strerror(ret)).url(url);
  }
  ret = curl_easy_setopt(curl, CURLOPT_FILETIME, 1L);
  if (ret != CURLE_OK) {
    throw curl_exception(curl_easy_strerror(ret)).url(url);
  }
  ret = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, impl::write_function);
  if (ret != CURLE_OK) {
    throw curl_exception(curl_easy_strerror(ret)).url(url);
  }
  ret = curl_easy_setopt(curl, CURLOPT_WRITEDATA, this);
  if (ret != CURLE_OK) {
    throw curl_exception(curl_easy_strerror(ret)).url(url);
  }
  ret = curl_easy_setopt(curl, CURLOPT_USERAGENT, "symboldb/0.0");
  if (ret != CURLE_OK) {
    throw curl_exception(curl_easy_strerror(ret)).url(url);
  }

  // The following settings should detect connectivity issues.  The
  // throughput limit is fairly low, but it should allow us to detect
  // dead connections.
  ret = curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30L);
  if (ret != CURLE_OK) {
    throw curl_exception(curl_easy_strerror(ret)).url(url);
  }
  ret = curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 500L);
  if (ret != CURLE_OK) {
    throw curl_exception(curl_easy_strerror(ret)).url(url);
  }
  ret = curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 60L);
  if (ret != CURLE_OK) {
    throw curl_exception(curl_easy_strerror(ret)).url(url);
  }

  CURLMcode mret = curl_multi_add_handle(multi_, curl);
  if (mret != CURLM_OK) {
    throw curl_exception(curl_multi_strerror(mret)).url(url);
  }

  connected_ = true;
}

size_t
cxxll::url_source::impl::read(unsigned char *buffer, size_t length)
{
  if (pending_pos_ < pending_.size()) {
    size_t to_copy =
      std::min(pending_.size() - pending_pos_, length);
    memcpy(buffer, pending_.data() + pending_pos_, to_copy);
    pending_pos_ += to_copy;
    return to_copy;
  }
  pending_.clear();
  pending_pos_ = 0;

  buffer_ = char_cast(buffer);
  buffer_length_ = length;
  buffer_written_ = 0;
  write_callback_ = false;

  // Clear out the target buffer reference on function exit.
  struct clearer {
    impl *impl_;
    ~clearer()
    {
      impl_->buffer_ = 0;
      impl_->buffer_length_ = 0;
      impl_->buffer_written_ = 0;
    }
  } clearer;
  clearer.impl_ = this;

  connect();

  size_t written;
  while (true) {
    CURLM *multi(multi_);
    int running_handles;
    CURLMcode ret = curl_multi_perform(multi, &running_handles);
    if (ret  != CURLM_OK) {
      throw curl_exception(curl_multi_strerror(ret)).url(url_);
    }
    written = buffer_written_;
    if (write_callback_ || running_handles == 0) {
      break;
    }

    // Wait for I/O activity.
    // TODO: This really should use poll instead of select, but this
    // will need wiring up another callback.
    fd_set rset, wset, eset;
    FD_ZERO(&rset);
    FD_ZERO(&wset);
    FD_ZERO(&eset);
    int max_fd;
    ret = curl_multi_fdset(multi, &rset, &wset, &eset, &max_fd);
    if (ret != CURLM_OK) {
      throw curl_exception(curl_multi_strerror(ret)).url(url_);
    }
    if (max_fd < 0) {
      // Yuck.  Curl requested busy-waiting.
      ::usleep(10 * 1000);
      continue;
    }
    long timeout;
    ret = curl_multi_timeout(multi, &timeout);
    if (ret  != CURLM_OK) {
      throw curl_exception(curl_multi_strerror(ret)).url(url_);
    }
    struct timeval tv;
    tv.tv_sec = timeout / 1000;
    tv.tv_usec = timeout * 1000;
    int osret = ::select(max_fd + 1, &rset, &wset, &eset, &tv);
    if (osret < 0) {
      throw os_exception().function(::select).count(max_fd);
    }
  }

  check();

  return written;
}

size_t
cxxll::url_source::impl::write_function(char *ptr, size_t size, size_t nmemb,
					void *userdata)
{
  impl &impl_(*static_cast<impl *>(userdata));
  size_t written = impl_.buffer_written_;
  size_t remaining = impl_.buffer_length_ - written;
  size_t total_size = size * nmemb;
  size_t to_copy = std::min(total_size, remaining);
  memcpy(impl_.buffer_ + written, ptr, to_copy);
  impl_.buffer_written_ = written + to_copy;
  impl_.write_callback_ = true;

  // Store any excess data for future reference.
  assert(impl_.pending_pos_ == 0);  // We would overwrite any pending data.
  impl_.pending_.insert(impl_.pending_.end(), ptr + to_copy, ptr + total_size);

  return total_size;
}

//////////////////////////////////////////////////////////////////////
// cxxll::url_source

cxxll::url_source::url_source(const char *url)
  : impl_(new impl(url))
{
}

cxxll::url_source::url_source(const std::string &url)
  : impl_(new impl(url))
{
}

cxxll::url_source::~url_source()
{
}

void
cxxll::url_source::connect()
{
  impl_->connect();
  // Establish the connection, saving some data for later reading.
  impl_->read(NULL, 0);
}

size_t
cxxll::url_source::read(unsigned char *buffer, size_t length)
{
  return impl_->read(buffer, length);
}

long long
cxxll::url_source::http_date() const
{
  if (impl_->before_data_) {
    return -1;
  }
  return impl_->http_date_;
}

long long
cxxll::url_source::file_size() const
{
  if (impl_->before_data_) {
    return -1;
  }
  return impl_->file_size_;
}

//////////////////////////////////////////////////////////////////////
// Global functions

void
cxxll::url_source_init()
{
  if (curl_global_init(CURL_GLOBAL_ALL) != 0) {
    raise<std::runtime_error>("libcurl initialization failed");
  }
}

void
cxxll::url_source_deinit()
{
  curl_global_cleanup();
}

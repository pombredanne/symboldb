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

#include "curl_handle.hpp"
#include "curl_fetch_result.hpp"
#include "curl_exception.hpp"
#include "sink.hpp"

#include <cstring>

size_t
curl_fetch_result::write_function(char *ptr, size_t size, size_t nmemb,
				  void *userdata)
{
  curl_fetch_result &r(*static_cast<curl_fetch_result *>(userdata));
  size = size * nmemb;
  if (r.error.empty()) {
    try {
      r.target->write(reinterpret_cast<unsigned char *>(ptr), size);
    } catch (std::exception &e) {
      r.error = e.what();		// FIXME: handle bad_alloc
    }
  }
  // Report the data as processed.
  return size;
}

void
curl_fetch_result::init(curl_handle &h, const char *url)
{
  error.clear();
  effective_url.clear();
  http_date = -1;
  http_size = -1;

  CURLcode ret;
  ret = curl_easy_setopt(h.raw, CURLOPT_URL, url);
  if (ret != CURLE_OK) {
    throw curl_exception(curl_easy_strerror(ret)).url(url);
  }
  ret = curl_easy_setopt(h.raw, CURLOPT_NOSIGNAL, 1L);
  if (ret != CURLE_OK) {
    throw curl_exception(curl_easy_strerror(ret)).url(url);
  }
  long protocols = CURLPROTO_HTTP | CURLPROTO_HTTPS | CURLPROTO_FTP;
  ret = curl_easy_setopt(h.raw, CURLOPT_REDIR_PROTOCOLS, protocols);
  if (ret != CURLE_OK) {
    throw curl_exception(curl_easy_strerror(ret)).url(url);
  }
  ret = curl_easy_setopt(h.raw, CURLOPT_FOLLOWLOCATION, 1L);
  if (ret != CURLE_OK) {
    throw curl_exception(curl_easy_strerror(ret)).url(url);
  }
  ret = curl_easy_setopt(h.raw, CURLOPT_FILETIME, 1L);
  if (ret != CURLE_OK) {
    throw curl_exception(curl_easy_strerror(ret)).url(url);
  }
  ret = curl_easy_setopt(h.raw, CURLOPT_WRITEFUNCTION, write_function);
  if (ret != CURLE_OK) {
    throw curl_exception(curl_easy_strerror(ret)).url(url);
  }
  ret = curl_easy_setopt(h.raw, CURLOPT_WRITEDATA, this);
  if (ret != CURLE_OK) {
    throw curl_exception(curl_easy_strerror(ret)).url(url);
  }
  ret = curl_easy_setopt(h.raw, CURLOPT_USERAGENT, "symboldb/0.0");
  if (ret != CURLE_OK) {
    throw curl_exception(curl_easy_strerror(ret)).url(url);
  }
  ret = curl_easy_setopt(h.raw, CURLOPT_CONNECTTIMEOUT, 30L);
  if (ret != CURLE_OK) {
    throw curl_exception(curl_easy_strerror(ret)).url(url);
  }
}

void
curl_fetch_result::perform(curl_handle &h, const char *url)
{
  CURLcode ret;


  ret = curl_easy_perform(h.raw);
  char *effective_url_c = NULL;
  curl_easy_getinfo(h.raw, CURLINFO_EFFECTIVE_URL, &effective_url_c);
  if (strcmp(url, effective_url_c) != 0) {
    effective_url = effective_url_c;
  }
  long status;
  curl_easy_getinfo(h.raw, CURLINFO_RESPONSE_CODE, &status);
  // A response code of 0 is used if the protocol does not support
  // response codes.
  if (ret != CURLE_OK || (status != 200 && status != 0)) {
    if (effective_url.empty()) {
      throw curl_exception(curl_easy_strerror(ret)).url(url).status(status);
    } else {
      throw curl_exception(curl_easy_strerror(ret)).url(effective_url)
	.original_url(url).status(status);
    }
  }
  curl_easy_getinfo(h.raw, CURLINFO_FILETIME, &http_date);
  double size;
  curl_easy_getinfo(h.raw, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &size);
  http_size = size;
}

curl_fetch_result::curl_fetch_result(sink *t)
  : target(t)
{
}

curl_fetch_result::~curl_fetch_result()
{
}

void
curl_fetch_result::get(const char *url)
{
  curl_handle h;
  init(h, url);
  perform(h, url);
}

void
curl_fetch_result::head(const char *url)
{
  curl_handle h;
  init(h, url);
  CURLcode ret;
  ret = curl_easy_setopt(h.raw, CURLOPT_NOBODY, 1L);
  if (ret != CURLE_OK) {
    throw curl_exception(curl_easy_strerror(ret)).url(url);
  }
  perform(h, url);
}

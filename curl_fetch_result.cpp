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

namespace {
  bool 
  set_error(curl_fetch_result &r, CURLcode ret)
  {
    r.error =  curl_easy_strerror(ret);
    return false;
  }

  size_t
  write_function(char *ptr, size_t size, size_t nmemb, void *userdata)
  {
    curl_fetch_result &r(*static_cast<curl_fetch_result *>(userdata));
    size = size * nmemb;
    r.data.insert(r.data.end(), ptr, ptr + size);
    return size;
  }

  bool
  init(curl_fetch_result &r, curl_handle &h, const char *url)
  {
    r.data.clear();
    r.error.clear();
    r.http_date = -1;
    r.http_size = -1;

    CURLcode ret;
    ret = curl_easy_setopt(h.raw, CURLOPT_URL, url);
    if (ret != CURLE_OK) {
      return set_error(r, ret);
    }
    ret = curl_easy_setopt(h.raw, CURLOPT_NOSIGNAL, 1L);
    if (ret != CURLE_OK) {
      return set_error(r, ret);
    }
    long protocols = CURLPROTO_HTTP | CURLPROTO_HTTPS | CURLPROTO_FTP;
    ret = curl_easy_setopt(h.raw, CURLOPT_REDIR_PROTOCOLS, protocols);
    if (ret != CURLE_OK) {
      return set_error(r, ret);
    }
    ret = curl_easy_setopt(h.raw, CURLOPT_FOLLOWLOCATION, 1L);
    if (ret != CURLE_OK) {
      return set_error(r, ret);
    }
    ret = curl_easy_setopt(h.raw, CURLOPT_FILETIME, 1L);
    if (ret != CURLE_OK) {
      return set_error(r, ret);
    }
    ret = curl_easy_setopt(h.raw, CURLOPT_WRITEFUNCTION, write_function);
    if (ret != CURLE_OK) {
      return set_error(r, ret);
    }
    ret = curl_easy_setopt(h.raw, CURLOPT_WRITEDATA, &r);
    if (ret != CURLE_OK) {
      return set_error(r, ret);
    }
    ret = curl_easy_setopt(h.raw, CURLOPT_USERAGENT, "symboldb/0.0");
    if (ret != CURLE_OK) {
      return set_error(r, ret);
    }
    ret = curl_easy_setopt(h.raw, CURLOPT_CONNECTTIMEOUT, 30L);
    if (ret != CURLE_OK) {
      return set_error(r, ret);
    }
    return true;
  }

  bool
  perform(curl_fetch_result &r, curl_handle &h)
  {
    CURLcode ret;
    ret = curl_easy_perform(h.raw);
    if (ret != CURLE_OK) {
      return set_error(r, ret);
    }
    curl_easy_getinfo(h.raw, CURLINFO_FILETIME, &r.http_date);
    double size;
    curl_easy_getinfo(h.raw, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &size);
    r.http_size = size;
    return true;
  }
}

void
curl_fetch_result::get(const char *url)
{
  curl_handle h;
  init(*this, h, url) && perform(*this, h);
}

void
curl_fetch_result::head(const char *url)
{
  curl_handle h;
  if (!init(*this, h, url)) {
    return;
  }
  CURLcode ret;
  ret = curl_easy_setopt(h.raw, CURLOPT_NOBODY, 1L);
  if (ret != CURLE_OK) {
    set_error(*this, ret);
    return;
  }
  perform(*this, h);
}

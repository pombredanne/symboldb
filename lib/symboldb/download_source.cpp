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

#include <symboldb/download.hpp>
#include <symboldb/database.hpp>
#include <cxxll/url_source.hpp>
#include <cxxll/curl_exception.hpp>
#include <cxxll/vector_source.hpp>
#include <cxxll/vector_sink.hpp>

using namespace cxxll;

namespace {
  struct database_source : source {
    std::vector<unsigned char> data;
    vector_source src;

    database_source()
      : src(&data)
    {
    }
    
    size_t
    read(unsigned char *buf, size_t len)
    {
      size_t ret = src.read(buf, len);
      return ret;
    }
  };

  struct download_source : source {
    database &db;
    std::string url;
    // TODO: Make this a direct member once we have move support.
    std::tr1::shared_ptr<url_source> src;
    std::vector<unsigned char> buffer;

    explicit download_source(database &d, const char *u)
      : db(d), url(u), src(new url_source(url))
    {
      src->connect();
    }

    explicit download_source(database &d, const char *u,
			     std::tr1::shared_ptr<url_source> presrc)
      : db(d), url(u), src(presrc)
    {
    }

    size_t
    read(unsigned char *buf, size_t len)
    {
      size_t ret = src->read(buf, len);
      if (ret == 0) {
	// We got the entire data.  Store it in the database.
	db.url_cache_update(url.c_str(), buffer, src->http_date());
      } else {
	buffer.insert(buffer.end(), buf, buf + ret);
      }
      return ret;
    }
  };
}


std::tr1::shared_ptr<cxxll::source>
download(const download_options &opt, database &db,
	 const char *url)
{
  switch (opt.cache_mode) {
  case download_options::only_cache:
  case download_options::always_cache:
    {
      std::tr1::shared_ptr<database_source> result(new database_source);
      if (db.url_cache_fetch(url, result->data)) {
	return result;
      }
      if (opt.cache_mode == download_options::only_cache) {
	throw curl_exception("URL not in cache and network access disabled")
	  .url(url);
      }
    }
    break;
  case download_options::no_cache:
    return std::tr1::shared_ptr<cxxll::source>(new url_source(url));
  case download_options::check_cache:
    {
      std::tr1::shared_ptr<cxxll::url_source> net(new url_source(url));
      net->connect();
      if (net->http_date() > 0 && net->file_size() >= 0) {
	std::tr1::shared_ptr<database_source> result(new database_source);
	if (db.url_cache_fetch(url, static_cast<size_t>(net->file_size()),
			       net->http_date(), result->data)) {
	  return result;
	}
      }
      return std::tr1::shared_ptr<cxxll::source>(new download_source(db, url, net));
    }
  }

  return std::tr1::shared_ptr<cxxll::source>(new download_source(db, url));
}

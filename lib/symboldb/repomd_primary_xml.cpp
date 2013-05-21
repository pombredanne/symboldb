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

#include <symboldb/repomd.hpp>
#include <symboldb/download.hpp>
#include <cxxll/memory_range_source.hpp>
#include <cxxll/gunzip_source.hpp>
#include <cxxll/string_support.hpp>
#include <cxxll/url.hpp>
#include <cxxll/curl_exception.hpp>
#include <cxxll/base16.hpp>

#include <stdexcept>
#include <vector>

using namespace cxxll;

struct repomd::primary_xml::impl {
  std::string url_;
  std::tr1::shared_ptr<const std::vector<unsigned char> > compressed_;
  memory_range_source mrsource_;
  gunzip_source gzsource_;

  impl(const std::string &url,
       std::tr1::shared_ptr<const std::vector<unsigned char> > compressed)
    : url_(url), compressed_(compressed),
      mrsource_(compressed_->data(), compressed_->size()),
      gzsource_(&mrsource_)
  {
  }
};

repomd::primary_xml::primary_xml(const repomd &rp,
				 const download_options &opt, database &db)
{
  for (std::vector<repomd::entry>::const_iterator p = rp.entries.begin(),
	 end = rp.entries.end(); p != end; ++p) {
    if (p->type == "primary" && ends_with(p->href, ".xml.gz")) {
      download_options dopt(opt);
      {
	// Check the cache for staleness if the file name does not
	// contain the hash.
	std::string digest
	  (base16_encode(p->checksum.value.begin(), p->checksum.value.end()));
	if ((digest.empty() || p->href.find(digest) == std::string::npos)
	    && dopt.cache_mode == download_options::always_cache) {
	  dopt.cache_mode = download_options::check_cache;
	}
      }
      std::string entry_url(url_combine_yum(rp.base_url.c_str(), p->href.c_str()));
      std::tr1::shared_ptr<std::vector<unsigned char> > compressed
	(new std::vector<unsigned char>());
      download(dopt, db, entry_url.c_str(), *compressed);
      std::vector<unsigned char> digest
	(hash(p->checksum.type, *compressed));
      if (digest != p->checksum.value) {
	std::string msg("compressed data does not match ");
	msg += hash_sink::to_string(p->checksum.type);
	msg += " checksum (actual ";
	msg += base16_encode(digest.begin(), digest.end());
	msg += ", expected ";
	msg += base16_encode(p->checksum.value.begin(),
			     p->checksum.value.end());
	msg += ')';
	throw curl_exception(msg.c_str());
      }
      impl_.reset(new impl(entry_url, compressed));
      return;
    }
  }
  // FIXME: use a more specific exception
  throw std::runtime_error(rp.base_url + ": could not find primary.xml");
}

repomd::primary_xml::~primary_xml()
{
}

const std::string &
repomd::primary_xml::url()
{
  return impl_->url_;
}

size_t
repomd::primary_xml::read(unsigned char *buf, size_t len)
{
  return impl_->gzsource_.read(buf, len);
}

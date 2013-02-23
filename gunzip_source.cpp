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

#include "gunzip_source.hpp"
#include "zlib_inflate_exception.hpp"

#include <cassert>
#include <cstring>
#include <stdexcept>

#include <zlib.h>

enum {
  BUFFER_SIZE = 4096
};

struct gunzip_source::impl {
  source *source_;
  z_stream stream_;
  unsigned char buffer_[BUFFER_SIZE];
  bool end_seen_;

  impl(source *src)
    : source_(src), end_seen_(false)
  {
    memset(&stream_, 0, sizeof(stream_));
    stream_.next_in = buffer_;
    int ret = inflateInit2(&stream_, 16 + MAX_WBITS /* gzip */);
    if (ret != Z_OK) {
      throw std::bad_alloc();
    }
  }

  size_t read(unsigned char *buf, size_t length)
  {
    if (end_seen_ || length == 0) {
      return 0;
    }
    stream_.next_out = buf;
    stream_.avail_out = length;
    while (true) {
      if (stream_.avail_in == 0) {
	size_t ret = source_->read(buffer_, sizeof(buffer_));
	if (ret == 0 && end_seen_) {
	  return stream_.next_out - buf;
	}
	if (ret == 0) {
	  throw zlib_inflate_exception("unexpected end of stream");
	}
	stream_.next_in = buffer_;
	stream_.avail_in = ret;
      }
      int err = inflate(&stream_, Z_NO_FLUSH);
      switch (err) {
      case Z_OK:
	end_seen_ = false;
	if (stream_.avail_out != 0) {
	  continue;
	}
	return length;
      case Z_STREAM_END:
	// We cannot try to decompress more data because the
	// caller-supplied buffer might be full.  But we need to check
	// that we have reached the final end of the source stream.
	if (stream_.avail_in == 0) {
	  size_t ret = source_->read(buffer_, sizeof(buffer_));
	  if (ret != 0) {
	    stream_.next_in = buffer_;
	    stream_.avail_in = ret;
	    end_seen_ = false;
	  } else {
	    end_seen_ = true;
	    return stream_.next_out - buf;
	  }
	} else {
	  end_seen_ = false;
	}
	inflateReset(&stream_);
	break;
      default:
	throw zlib_inflate_exception(stream_.msg);
      }
    }
  }
};

gunzip_source::gunzip_source(source *src)
  : impl_(new impl(src))
{
}

gunzip_source::~gunzip_source()
{
}

size_t
gunzip_source::read(unsigned char *buf, size_t length)
{
  return impl_->read(buf, length);
}

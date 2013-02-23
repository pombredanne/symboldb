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

#include "zlib.hpp"

#include <cstddef>
#include <cstring>

#include <zlib.h>

/*
  Copyright (C) 1995-2012 Jean-loup Gailly and Mark Adler

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.

  Jean-loup Gailly        Mark Adler
  jloup@gzip.org          madler@alumni.caltech.edu
*/

// Adapted from uncompress() in uncompr.c.  Altered to support gzip
// decompression and a growing std::vector as output buffer.
bool gzip_uncompress(const std::vector<unsigned char> &in,
		     std::vector<unsigned char> &out)
{
  if (in.empty()) {
    return false;
  }

  z_stream stream;
  memset(&stream, 0, sizeof(stream));
  stream.next_in = const_cast<Bytef *>(in.data());
  stream.avail_in = in.size();
  if (in.size() != stream.avail_in) {
    return false;
  }

  out.resize(2 * in.size());

  int err = inflateInit2(&stream, 16 + MAX_WBITS /* gzip */);
  if (err != Z_OK) {
    return false;
  }
  try {
    while (true) {
      stream.next_out = out.data() + stream.total_out;
      stream.avail_out = out.size() - stream.total_out;

      err = inflate(&stream, Z_NO_FLUSH);
      switch (err) {
      case Z_STREAM_END:
	goto out;
      case Z_OK:
	{
	  size_t new_size = out.size() * 2;
	  if (new_size < out.size()) {
	    inflateEnd(&stream);
	    return false;
	  }
	  out.resize(new_size);
	}
	break;
      default:
	inflateEnd(&stream);
	return false;
      }
    }
  out:
    out.resize(stream.total_out);
  } catch(...) {
    inflateEnd(&stream);
    throw;
  }
  inflateEnd(&stream);
  return true;
}

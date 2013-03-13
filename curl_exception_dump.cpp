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

#include "curl_exception_dump.hpp"
#include "curl_exception.hpp"

void
dump(const char *prefix, const curl_exception &e, FILE *out)
{
  fprintf(out, "%sdownload", prefix);
  if (!e.remote_ip().empty()) {
    fprintf(out, " from [%s]:%u",
	    e.remote_ip().c_str(), e.remote_port());
  }
  if (e.status() != 0) {
    fprintf(out, " failed with status code %d\n", e.status());
    if (!e.message().empty()) {
      fprintf(out, "%s  %s\n", prefix, e.message().c_str());
    }
  } else if (!e.message().empty())  {
    fprintf(out, " failed: %s\n", e.message().c_str());
  } else {
    fprintf(out, " failed\n");
  }
  if (!e.url().empty()) {
    fprintf(out, "%s  URL: %s\n", prefix, e.url().c_str());
  }
  if (!e.original_url().empty() && e.original_url() != e.url()) {
    fprintf(out, "%s  starting at: %s\n", prefix, e.original_url().c_str());
  }
}

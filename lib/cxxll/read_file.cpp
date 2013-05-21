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

#include <cxxll/read_file.hpp>
#include <cxxll/fd_handle.hpp>
#include <cxxll/fd_source.hpp>
#include <cxxll/source_sink.hpp>
#include <cxxll/vector_sink.hpp>

void
cxxll::read_file(const char *path, std::vector<unsigned char> &target)
{
  fd_handle fd;
  fd.open_read_only(path);
  fd_source source(fd.get());
  vector_sink sink;
  sink.data.swap(target);
  try {
    copy_source_to_sink(source, sink);
  } catch (...) {
    sink.data.swap(target);
    throw;
  }
  sink.data.swap(target);
}

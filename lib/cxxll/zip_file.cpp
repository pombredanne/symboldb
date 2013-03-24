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

#include <cxxll/zip_file.hpp>
#include <cxxll/source.hpp>
#include <cxxll/source_sink.hpp>
#include <cxxll/vector_sink.hpp>
#include <cxxll/os_exception.hpp>

#include <stdexcept>

#include <archive.h>
#include <archive_entry.h>

bool
cxxll::zip_file::has_signature(const std::vector<unsigned char> &vec)
{
  return vec.size() > 4 && vec.at(0) == 'P' && vec.at(1) == 'K'
    && vec.at(2) == 3 && vec.at(3) == 4;
}

struct cxxll::zip_file::impl : source {
  const std::vector<unsigned char> *buffer_;
  archive *archive_;
  archive_entry *entry_;

  impl(const std::vector<unsigned char> *buffer);
  ~impl();

  size_t read(unsigned char *, size_t);
};

cxxll::zip_file::impl::impl(const std::vector<unsigned char> *buffer)
  : buffer_(buffer), archive_(archive_read_new())
{
  if (archive_ == NULL) {
    throw std::bad_alloc();
  }
  if (archive_read_support_format_zip(archive_) != ARCHIVE_OK
      || (archive_read_open_memory(archive_,
				   const_cast<unsigned char *>(buffer->data()),
				   buffer->size())
	  != ARCHIVE_OK)
      || (entry_ = archive_entry_new2(archive_)) == NULL) {
    archive_read_finish(archive_);
    throw std::runtime_error("archive_read_support_format_zip");
  }
}

cxxll::zip_file::impl::~impl()
{
  archive_entry_free(entry_);
  archive_read_finish(archive_);
}

size_t
cxxll::zip_file::impl::read(unsigned char *buffer, size_t length)
{
  ssize_t ret = archive_read_data(archive_, buffer, length);
  if (ret < 0) {
    throw os_exception().function(archive_read_data).length(length);
  }
  return ret;
}

cxxll::zip_file::zip_file(const std::vector<unsigned char> *buffer)
  : impl_(new impl(buffer))
{
}

cxxll::zip_file::~zip_file()
{
}

bool
cxxll::zip_file::next()
{
  int ret = archive_read_next_header2(impl_->archive_, impl_->entry_);
  if (ret == ARCHIVE_EOF) {
    return false;
  } else if (ret != ARCHIVE_OK) {
    throw std::runtime_error("archive_read_next_header2");
  }
  return true;
}

std::string
cxxll::zip_file::name() const
{
  return archive_entry_pathname(impl_->entry_);
}

void
cxxll::zip_file::data(std::vector<unsigned char> &target)
{
  target.clear();
  vector_sink sink;
  sink.data.swap(target);
  try {
    copy_source_to_sink(*impl_, sink);
  } catch (...) {
    sink.data.swap(target);
    throw;
  }
  sink.data.swap(target);
}

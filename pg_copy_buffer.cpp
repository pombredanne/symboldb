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

#include "pg_copy_buffer.hpp"
#include "pgconn_handle.hpp"
#include "pgresult_handle.hpp"
#include "pg_private.hpp"
#include "endian.hpp"

struct pg_copy_buffer::impl {
  pgconn_handle *conn;
  std::string sql;
  std::vector<char> buffer;
  bool in_row;

  impl(pgconn_handle *, const char *command);
  void send_buffer();
  void store_length(unsigned);

  template <class T> void column(const T &);
};

pg_copy_buffer::impl::impl(pgconn_handle *c, const char *command)
  : conn(c), sql(command), in_row(false)
{
  buffer.reserve(500000);
}

void
pg_copy_buffer::impl::send_buffer()
{
  if (!buffer.empty()) {
    conn->putCopyData(buffer.data(), buffer.size());
    buffer.resize(0);
  }
}

void
pg_copy_buffer::impl::store_length(unsigned length)
{
  union {
    unsigned number;
    char buf[sizeof(unsigned)];
  } u;
  u.number = cpu_to_be_32(length);
  buffer.insert(buffer.end(), u.buf, u.buf + sizeof(u.buf));
}

template <class  T> inline void
pg_copy_buffer::impl::column(const T &val)
{
  char storage[pg_private::dispatch<T>::storage];
  const char *pbuffer = pg_private::dispatch<T>::store(storage, val);
  int length = pg_private::dispatch<T>::length(val);
  size_t capacity = buffer.capacity();
  size_t buffer_size = buffer.size();
  if (4 > capacity - buffer_size) {
    send_buffer();
  }
  if (pbuffer == NULL) {
    // NULL column.
    store_length(-1);
  } else {
    store_length(length);
    if (static_cast<size_t>(length) > capacity - buffer.size()) {
      send_buffer();
      conn->putCopyData(pbuffer, length);
    } else {
      buffer.insert(buffer.end(), pbuffer, pbuffer + length);
    }
  }
}

pg_copy_buffer::pg_copy_buffer(pgconn_handle *conn, const char *command)
  : impl_(new impl(conn, command))
{
}

pg_copy_buffer::~pg_copy_buffer()
{
}

pg_copy_buffer &
pg_copy_buffer::column(bool value)
{
  impl_->column(value);
  return *this;
}

pg_copy_buffer &
pg_copy_buffer::column(short value)
{
  impl_->column(value);
  return *this;
}

pg_copy_buffer &
pg_copy_buffer::column(int value)
{
  impl_->column(value);
  return *this;
}

pg_copy_buffer &
pg_copy_buffer::column(long long value)
{
  impl_->column(value);
  return *this;
}

pg_copy_buffer &
pg_copy_buffer::column(const char *value)
{
  impl_->column(value);
  return *this;
}

pg_copy_buffer &
pg_copy_buffer::column(const std::string &value)
{
  impl_->column(value);
  return *this;
}

pg_copy_buffer &
pg_copy_buffer::column(const std::vector<unsigned char> &value)
{
  impl_->column(value);
  return *this;
}

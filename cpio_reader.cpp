/*
 * Copyright (C) 2012 Red Hat, Inc.
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

#include "cpio_reader.hpp"

#include <assert.h>
#include <stddef.h>
#include <string.h>

static int
octal_digit(char ch)
{
  if (ch < '0' || ch > '7') {
    return -1;
  }
  return ch - '0';
}

static int
hex_digit(char ch)
{
  if (ch >= '0' && ch <= '9') {
    return ch - '0';
  } else if (ch >= 'a' && ch <= 'f') {
    return ch - 'a' + 10;
  } else if (ch >= 'A' && ch <= 'F') {
    return ch - 'A' + 10;
  }
  return -1;
}

template <unsigned N>
static const char *
read_octal(const char *where, const char *p, uint32_t &result, const char *&error)
{
  if (p == NULL) {
    return NULL;
  }

  result = 0;
  for (unsigned i = 0; i < N; ++i) {
    int d = octal_digit(p[i]);
    if (d < 0) {
      error = where;
      return NULL;
    }
    result = (result << 3) | d;
  }
  return p + N;
}

static const char *
read_hex(const char *where, const char *p, uint32_t &result, const char *&error)
{
  if (p == NULL) {
    return NULL;
  }

  result = 0;
  for (unsigned i = 0; i < 8; ++i) {
    int d = hex_digit(p[i]);
    if (d < 0) {
      error = where;
      return NULL;
    }
    result = (result << 4) | d;
  }
  return p + 8;
}

size_t
cpio_header_length(const char magic[6])
{
  if (memcmp(magic, "070707", 6) == 0) {
    return 70;
  } else if (memcmp(magic, "070701", 6) == 0) {
    return 104;
  } else {
    return 0;
  }
}


bool
parse(const char *buf, size_t len, cpio_entry &e, const char *&error)
{
  const char *p = buf;
  switch (len) {
  case 70:
    p = read_octal<6>("dev", p, e.devmajor, error);
    e.devminor = 0;
    p = read_octal<6>("ino", p, e.ino, error);
    p = read_octal<6>("mode", p, e.mode, error);
    p = read_octal<6>("uid", p, e.uid, error);
    p = read_octal<6>("gid", p, e.gid, error);
    p = read_octal<6>("nlink", p, e.nlink, error);
    p = read_octal<6>("rdev", p, e.rdevmajor, error);
    e.rdevminor = 0;
    p = read_octal<11>("mtime", p, e.mtime, error);
    p = read_octal<6>("namesize", p, e.namesize, error);
    p = read_octal<11>("filesize", p, e.filesize, error);
    if (p != NULL) {
      assert(buf + 70 == p);
      return true;
    }
    break;
  case 104:
    p = read_hex("ino", p, e.ino, error);
    p = read_hex("mode", p, e.mode, error);
    p = read_hex("uid", p, e.uid, error);
    p = read_hex("gid", p, e.gid, error);
    p = read_hex("nlink", p, e.nlink, error);
    p = read_hex("mtime", p, e.mtime, error);
    p = read_hex("filesize", p, e.filesize, error);
    p = read_hex("devmajor", p, e.devmajor, error);
    p = read_hex("devminor", p, e.devminor, error);
    p = read_hex("rdevmajor", p, e.rdevmajor, error);
    p = read_hex("rdevminor", p, e.rdevminor, error);
    p = read_hex("namesize", p, e.namesize, error);
    p = read_hex("check", p, e.check, error);
    if (p != NULL) {
      assert(buf + 104 == p);
      return true;
    }
    break;
  default:
    error = "magic";
  }
  return false;
}

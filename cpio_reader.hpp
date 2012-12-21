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

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <cpio.h>

struct cpio_entry {
  static const size_t magic_size = 6;
  uint32_t devmajor;
  uint32_t devminor;
  uint32_t ino;
  uint32_t mode;
  uint32_t uid;
  uint32_t gid;
  uint32_t nlink;
  uint32_t rdevmajor;
  uint32_t rdevminor;
  uint32_t mtime;
  uint32_t namesize;
  uint32_t filesize;
  uint32_t check;
};

// Calculates the header length for string, excluding the magic
// string.  Returns 0 on error.
size_t cpio_header_length(const char magic[cpio_entry::magic_size]);

// Parses cpio header into the fields above.  The magic field is not
// included.  Returns true on success, sets ERROR to the name of the
// broken field on error.
bool parse(const char *buf, size_t len, cpio_entry &e,
	   const char *&error);


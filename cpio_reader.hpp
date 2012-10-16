#pragma once

#include <cpio.h>
#include <inttypes.h>

struct cpio_entry {
  uint32_t magic;
  uint32_t dev;
  uint32_t ino;
  uint32_t mode;
  uint32_t uid;
  uint32_t gid;
  uint32_t nlink;
  uint32_t rdev;
  uint64_t mtime;
  uint32_t namesize;
  uint64_t filesize;
};

// Parses a 76-byte header into the fields above.  Returns true on
// success, sets ERROR to the name of the broken field on error.
bool parse(const char[76], cpio_entry &, const char *&error);


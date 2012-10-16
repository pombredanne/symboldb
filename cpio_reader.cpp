#include "cpio_reader.hpp"

#include <assert.h>
#include <stddef.h>

static int
digit(char ch)
{
  if (ch < '0' || ch > '7') {
    return -1;
  }
  return ch - '0';
}

template <class T>
static const char *
read_octal(const char *where, const char *p, T &result, const char *&error)
{
  if (p == NULL) {
    return NULL;
  }

  unsigned n;
  switch (sizeof(T)) {
  case 4:
    n = 6;
    break;
  case 8:
    n = 11;
    break;
  default:
    assert(false);
  }

  result = 0;
  for (unsigned i = 0; i < n; ++i) {
    int d = digit(p[i]);
    if (d < 0) {
      error = where;
    }
    result = (result << 3) | d;
  }
  return p + n;
}

bool
parse(const char buf[76], cpio_entry &e, const char *&error)
{
  const char *p = read_octal("magic", buf, e.magic, error);
  p = read_octal("dev", p, e.dev, error);
  p = read_octal("ino", p, e.ino, error);
  p = read_octal("mode", p, e.mode, error);
  p = read_octal("uid", p, e.uid, error);
  p = read_octal("gid", p, e.gid, error);
  p = read_octal("nlink", p, e.nlink, error);
  p = read_octal("rdev", p, e.rdev, error);
  p = read_octal("mtime", p, e.mtime, error);
  p = read_octal("namesize", p, e.namesize, error);
  p = read_octal("filesize", p, e.filesize, error);
  if (p != NULL) {
    assert(buf + 76 == p);
    return p;
  }
  return false;
}

#include "rpm_parser.hpp"

#include <rpm/rpmlib.h>
#include <rpm/rpmts.h>

void
rpm_parser_init()
{
  rpmReadConfigFiles("", "noarch");
}

// The code below roughly follows rpm2cpio.

struct rpm_parser_state::impl {
  FD_t fd;
  Header header;
  FD_t gzfd;
};

rpm_parser_state::rpm_parser_state(const char *path)
  : impl_(new impl)
{
  impl_->fd = Fopen(path, "r.ufdio");
  try {
    if (Ferror(impl_->fd)) {
      throw rpm_parser_exception(Fstrerror(impl_->fd));
    }

    // Load header.
    rpmts ts = rpmtsCreate();
    int rc = rpmReadPackageFile(ts, impl_->fd, "symboldb", &impl_->header);
    ts = rpmtsFree(ts);
    switch (rc) {
    case RPMRC_OK:
    case RPMRC_NOKEY:
    case RPMRC_NOTTRUSTED:
      break;
    case RPMRC_NOTFOUND:
      throw rpm_parser_exception("not an RPM package");
    case RPMRC_FAIL:
    default:
      throw rpm_parser_exception("error reading header from RPM package");
    }

    // Open payload stream.
    try {
      const char *compr =
	headerGetString(impl_->header, RPMTAG_PAYLOADCOMPRESSOR);
      std::string rpmio_flags("r.");
      if (compr == NULL) {
	rpmio_flags += "gzip";
      } else {
	rpmio_flags += compr;
      }
      impl_->gzfd = Fdopen(impl_->fd, rpmio_flags.c_str());
      try {
	if (Ferror(impl_->gzfd)) {
	  throw rpm_parser_exception(Fstrerror(impl_->gzfd));
	}
      } catch (...) {
	Fclose(impl_->gzfd);
	throw;
      }
    } catch (...) {
      headerFree(impl_->header);
      throw;
    }
  } catch (...) {
    Fclose(impl_->fd);
    throw;
  }
}

rpm_parser_state::~rpm_parser_state()
{
  headerFree(impl_->header);
  Fclose(impl_->fd);
}

bool
rpm_parser_state::read_file(cpio_entry &entry,
			    std::vector<char> &name,
			    std::vector<char> &contents)
{
  char cpio_magic[cpio_entry::magic_size];
  ssize_t ret = Fread(cpio_magic, sizeof(cpio_magic), 1, impl_->gzfd);
  if (ret == 0) {
    throw rpm_parser_exception("end of stream in cpio file header");
  } else if (ret < 0) {
    throw rpm_parser_exception(std::string(Fstrerror(impl_->gzfd))
			       + " (in cpio header)");
  }
  size_t cpio_len = cpio_header_length(cpio_magic);
  if (cpio_len == 0) {
    throw rpm_parser_exception("unknown cpio version");
  }
  char cpio_header[cpio_len];
  ret = Fread(cpio_header, cpio_len, 1, impl_->gzfd);
  if (ret == 0) {
    throw rpm_parser_exception("end of stream in cpio file header");
  } else if (ret < 0) {
    throw rpm_parser_exception(std::string(Fstrerror(impl_->gzfd))
			       + " (in cpio header)");
  }

  const char *error;
  if (!parse(cpio_header, cpio_len, entry, error)) {
    throw rpm_parser_exception(std::string("malformed cpio header field: ") 
			       + error);
  }
  if (entry.namesize == 0) {
    throw rpm_parser_exception("empty file name in cpio header");
  }

  // Read name.
  name.resize(entry.namesize);
  ret = Fread(&name.front(), entry.namesize, 1, impl_->gzfd);
  if (ret == 0) {
    throw rpm_parser_exception("end of stream in cpio file name");
  } else if (ret < 0) {
    throw rpm_parser_exception(std::string(Fstrerror(impl_->gzfd))
			       + " (in cpio file name)");
  }
  // Name padding.
  unsigned pos = 6 + cpio_len + name.size();
  while ((pos % 4) != 0) {
    char buf;
    ret = Fread(&buf, 1, 1, impl_->gzfd);
    if (ret == 0) {
      throw rpm_parser_exception("failed to read padding after name");
    } else if (ret < 0) {
      throw rpm_parser_exception(std::string(Fstrerror(impl_->gzfd))
				 + " (in cpio file name padding)");
    }
    ++pos;
  }
  
  // Read contents.
  contents.resize(entry.filesize);
  if (entry.filesize > 0) {
    ret = Fread(&contents.front(), entry.filesize, 1, impl_->gzfd);
    if (ret == 0) {
      throw rpm_parser_exception("end of stream in cpio file contents");
    } else if (ret < 0) {
      throw rpm_parser_exception(std::string(Fstrerror(impl_->gzfd))
				 + " (in cpio file contents)");
    }
  }
  // Contents padding.
  pos = entry.filesize;
  while ((pos % 4) != 0) {
    char buf;
    ret = Fread(&buf, 1, 1, impl_->gzfd);
    if (ret == 0) {
      throw rpm_parser_exception("failed to read padding after cpio contents");
    } else if (ret < 0) {
      throw rpm_parser_exception(std::string(Fstrerror(impl_->gzfd))
				 + " (in cpio file contents padding)");
    }
    ++pos;
  }

  return true;
}

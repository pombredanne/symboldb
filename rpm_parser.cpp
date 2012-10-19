#include "rpm_parser.hpp"
#include "cpio_reader.hpp"
#include "rpm_file_info.hpp"
#include "rpmtd_wrapper.hpp"

#include <assert.h>

#include <rpm/rpmlib.h>
#include <rpm/rpmts.h>

#include <map>
#include <tr1/memory>

void
rpm_parser_init()
{
  rpmReadConfigFiles("", "noarch");
}

struct rpm_parser_state::impl {
  FD_t fd;
  Header header;
  FD_t gzfd;
  rpmtd_wrapper nevra;

  typedef std::map<std::string, std::tr1::shared_ptr<rpm_file_info> > file_map;
  file_map files;
  void get_files_from_header();
};

// Missing from <rpm/rpmtd.h>, see rpmtdNextUint32.
static uint16_t *
tdNextUint16(rpmtd td)
{
    assert(td != NULL);
    uint16_t *res = NULL;
    if (rpmtdNext(td) >= 0) {
	res = rpmtdGetUint16(td);
    }
    return res;
}


void
rpm_parser_state::impl::get_files_from_header()
{
  const headerGetFlags hflags = HEADERGET_ALLOC | HEADERGET_EXT;

  rpmtd_wrapper names;
  if (!headerGet(header, RPMTAG_FILENAMES, names.raw, hflags)) {
    throw rpm_parser_exception("could not get FILENAMES header");
  }
  rpmtd_wrapper users;
  if (!headerGet(header, RPMTAG_FILEUSERNAME, users.raw, hflags)) {
    throw rpm_parser_exception("could not get FILEUSERNAME header");
  }
  rpmtd_wrapper groups;
  if (!headerGet(header, RPMTAG_FILEGROUPNAME, groups.raw, hflags)) {
    throw rpm_parser_exception("could not get FILEGROUPNAME header");
  }
  rpmtd_wrapper mtimes;
  if (!headerGet(header, RPMTAG_FILEMTIMES, mtimes.raw, hflags)) {
    throw rpm_parser_exception("could not get FILEMTIMES header");
  }
  rpmtd_wrapper modes;
  if (!headerGet(header, RPMTAG_FILEMODES, modes.raw, hflags)) {
    throw rpm_parser_exception("could not get FILEMODES header");
  }

  while (true) {
    const char *name = rpmtdNextString(names.raw);
    if (name == NULL) {
      break;
    }
    const char *user = rpmtdNextString(users.raw);
    if (name == NULL) {
      throw rpm_parser_exception("missing entries in FILEUSERNAME header");
    }
    const char *group = rpmtdNextString(groups.raw);
    if (name == NULL) {
      throw rpm_parser_exception("missing entries in FILEUSERGROUP header");
    }
    const uint32_t *mtime = rpmtdNextUint32(mtimes.raw);
    if (mtime == NULL) {
      throw rpm_parser_exception("missing entries in FILEMTIMES header");
    }
    const uint16_t *mode = tdNextUint16(modes.raw);
    if (mode == NULL) {
      throw rpm_parser_exception("missing entries in FILEMODES header");
    }

    std::tr1::shared_ptr<rpm_file_info> p(new rpm_file_info);
    p->name = name;
    p->user = user;
    p->group = group;
    p->mtime = *mtime;
    p->mode = *mode;
    files[p->name] = p;
  }

  if (rpmtdNextString(users.raw) != NULL) {
    throw rpm_parser_exception
      ("FILEUSERNAME header contains too many elements");
  }
  if (rpmtdNextString(groups.raw) != NULL) {
    throw rpm_parser_exception
      ("FILEGROUPNAME header contains too many elements");
  }
  if (rpmtdNextUint32(mtimes.raw) != NULL) {
    throw rpm_parser_exception
      ("FILEMTIMES header contains too many elements");
  }
  if (rpmtdNextUint32(modes.raw) != NULL) {
    throw rpm_parser_exception
      ("FILEMODES header contains too many elements");
  }
}

rpm_parser_state::rpm_parser_state(const char *path)
  : impl_(new impl)
{
  // The code below roughly follows rpm2cpio.
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

    if (!headerGet(impl_->header, RPMTAG_NEVRA, impl_->nevra.raw,
		   HEADERGET_EXT)) {
      throw rpm_parser_exception("could not get NEVRA header from RPM package");
    }
    impl_->get_files_from_header();

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

const char *
rpm_parser_state::nevra() const
{
  return rpmtdGetString(impl_->nevra.raw);
}

bool
rpm_parser_state::read_file(rpm_file_entry &file)
{
  // Read cpio header magic.
  char cpio_magic[cpio_entry::magic_size];
  ssize_t ret = Fread(cpio_magic, sizeof(cpio_magic), 1, impl_->gzfd);
  if (ret == 0) {
    throw rpm_parser_exception("end of stream in cpio file header");
  } else if (ret < 0) {
    throw rpm_parser_exception(std::string(Fstrerror(impl_->gzfd))
			       + " (in cpio header)");
  }

  // Determine cpio header length.
  size_t cpio_len = cpio_header_length(cpio_magic);
  if (cpio_len == 0) {
    throw rpm_parser_exception("unknown cpio version");
  }

  // Read cpio header.
  char cpio_header[128];
  assert(cpio_len <= sizeof(cpio_header));
  ret = Fread(cpio_header, cpio_len, 1, impl_->gzfd);
  if (ret == 0) {
    throw rpm_parser_exception("end of stream in cpio file header");
  } else if (ret < 0) {
    throw rpm_parser_exception(std::string(Fstrerror(impl_->gzfd))
			       + " (in cpio header)");
  }

  // Parse cpio header.
  cpio_entry header;
  const char *error;
  if (!parse(cpio_header, cpio_len, header, error)) {
    throw rpm_parser_exception(std::string("malformed cpio header field: ") 
			       + error);
  }
  if (header.namesize == 0) {
    throw rpm_parser_exception("empty file name in cpio header");
  }

  // Read name.
  std::vector<char> name;
  name.resize(header.namesize);
  ret = Fread(&name.front(), header.namesize, 1, impl_->gzfd);
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

  // Check for end marker.
  const char *trailer = "TRAILER!!!";
  if (name.size() == strlen(trailer) + 1
      && memcmp(&name.front(), trailer, strlen(trailer)) == 0) {
    return false;
  }

  // Normalize file name.
  char *name_normalized = &name.front();
  if (name.size() >= 2 && name.at(0) == '.' && name.at(1) == '/') {
    ++name_normalized;
  }

  // Obtain file information.
  {
    impl::file_map::const_iterator p = impl_->files.find(name_normalized);
    if (p == impl_->files.end()) {
      throw rpm_parser_exception
	(std::string("cpio file not found in RPM header: ")
	 + &name.front());
    }
    file.info = p->second;
  }
  
  // Read contents.
  file.contents.resize(header.filesize);
  if (header.filesize > 0) {
    ret = Fread(&file.contents.front(), header.filesize, 1, impl_->gzfd);
    if (ret == 0) {
      throw rpm_parser_exception("end of stream in cpio file contents");
    } else if (ret < 0) {
      throw rpm_parser_exception(std::string(Fstrerror(impl_->gzfd))
				 + " (in cpio file contents)");
    }
  }

  // Contents padding.
  pos = header.filesize;
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

/*
 * Copyright (C) 2012, 2013 Red Hat, Inc.
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

#include <cxxll/rpm_parser.hpp>
#include <cxxll/rpm_parser_exception.hpp>
#include <cxxll/cpio_reader.hpp>
#include <cxxll/rpm_file_info.hpp>
#include <cxxll/rpm_package_info.hpp>
#include <cxxll/rpmtd_wrapper.hpp>

#include <assert.h>
#include <limits.h>

#include <rpm/rpmlib.h>
#include <rpm/rpmlog.h>
#include <rpm/rpmpgp.h>
#include <rpm/rpmts.h>

#include <map>
#include <tr1/memory>

using namespace cxxll;

void
cxxll::rpm_parser_init()
{
  rpmReadConfigFiles("", "noarch");
}

void
cxxll::rpm_parser_deinit()
{
  rpmInitCrypto();
  rpmFreeCrypto();
}

struct rpm_parser_state::impl {
  FD_t fd;
  Header header;
  std::vector<rpm_dependency> dependencies;
  bool payload_is_open;

  impl()
    : fd(0), header(0), payload_is_open(false)
  {
  }

  ~impl()
  {
    if (header != NULL) {
      headerFree(header);
    }
    if (fd != NULL) {
      Fclose(fd);
    }
  }

  rpmtd_wrapper nevra;
  rpm_package_info pkg;
  std::string digest_algo;

  typedef std::map<std::string, std::tr1::shared_ptr<rpm_file_info> > file_map;
  file_map files;
  void get_header();
  void get_files_from_header(); // called on demand by open_payload()
  void get_dependencies(); // populate the dependencies member
  void open_payload(); // called on demand by read_file()
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

static bool
header_present(Header header, rpmTagVal tag)
{
  rpmtd_wrapper td;
  return headerGet(header, tag, td.raw, HEADERGET_EXT);
}

static std::string
get_string(Header header, const char *name, rpmTagVal tag)
{
  rpmtd_wrapper td;
  if (headerGet(header, tag, td.raw, 0)) {
    const char *s = rpmtdGetString(td.raw);
    if (s != NULL) {
      return s;
    }
  }
  throw rpm_parser_exception(std::string("could not get RPM ")
			     + name + " header");
}

static unsigned
get_unsigned(Header header, const char *name, rpmTagVal tag)
{
  rpmtd_wrapper td;
  unsigned *p;
  if (!headerGet(header, tag, td.raw, 0)
      || (p = rpmtdGetUint32(td.raw)) == NULL) {
    throw rpm_parser_exception
      ("could not get " + std::string(name) + " header");
  }
  return *p;
}

void
rpm_parser_state::impl::get_header()
{
  if (!headerGet(header, RPMTAG_NEVRA, nevra.raw, HEADERGET_EXT)) {
    throw rpm_parser_exception("could not get NEVRA header from RPM package");
  }

  pkg.name = get_string(header, "NAME", RPMTAG_NAME);
  pkg.version = get_string(header, "VERSION", RPMTAG_VERSION);
  pkg.release = get_string(header, "RELEASE", RPMTAG_RELEASE);
  pkg.arch = get_string(header, "ARCH", RPMTAG_ARCH);
  pkg.source_rpm = get_string(header, "SOURCERPM", RPMTAG_SOURCERPM);
  pkg.hash = get_string(header, "SHA1HEADER", RPMTAG_SHA1HEADER);
  pkg.build_host = get_string(header, "BUILDHOST", RPMTAG_BUILDHOST);
  pkg.summary = get_string(header, "SUMMARY", RPMTAG_SUMMARY);
  pkg.description = get_string(header, "DESCRIPTION", RPMTAG_DESCRIPTION);
  pkg.license = get_string(header, "LICENSE", RPMTAG_LICENSE);
  pkg.group = get_string(header, "GROUP", RPMTAG_GROUP);

  {
    rpmtd_wrapper td;
    if (!headerGet(header, RPMTAG_EPOCH, td.raw, 0)) {
      pkg.epoch = -1;
    } else {
      unsigned *p = rpmtdGetUint32(td.raw);
      if (p == NULL) {
	throw rpm_parser_exception("could not get EPOCH header");
      }
      if (*p > INT_MAX) {
	throw rpm_parser_exception("RPM epoch out of range");
      }
      pkg.epoch = *p;
    }
  }

  pkg.build_time = get_unsigned(header, "BUILDTIME", RPMTAG_BUILDTIME);
  {
    unsigned digalg;
    try {
      digalg = get_unsigned(header, "FILEDIGESTALGO", RPMTAG_FILEDIGESTALGO);
    } catch (rpm_parser_exception &) {
      digalg = PGPHASHALGO_MD5;
    }
    switch (digalg) {
    case PGPHASHALGO_MD5:
      digest_algo = "md5";
      break;
    case PGPHASHALGO_SHA1:
      digest_algo = "sha1";
      break;
    case PGPHASHALGO_SHA256:
      digest_algo = "sha256";
      break;
    default:
      {
	char buf[128];
	snprintf(buf, sizeof(buf), "unknown file digest algorithm %u", digalg);
	throw rpm_parser_exception(buf);
      }
    }
  }
}

void
rpm_parser_state::impl::get_files_from_header()
{
  const headerGetFlags hflags = HEADERGET_ALLOC | HEADERGET_EXT;

  rpmtd_wrapper names;
  if (!headerGet(header, RPMTAG_FILENAMES, names.raw, hflags)) {
    if (header_present(header, RPMTAG_FILEUSERNAME)
	|| header_present(header, RPMTAG_FILEGROUPNAME)
	|| header_present(header, RPMTAG_FILEMTIMES)
	|| header_present(header, RPMTAG_FILEMODES)) {
      throw rpm_parser_exception("could not get FILENAMES header");
    } else {
      // Nothing to do, empty package.
      return;
    }
  }
  rpmtd_wrapper sizes;
  if (!headerGet(header, RPMTAG_LONGFILESIZES, sizes.raw, hflags)) {
    throw rpm_parser_exception("could not get LONGFILESIZES header");
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
  rpmtd_wrapper inodes;
  if (!headerGet(header, RPMTAG_FILEINODES, inodes.raw, hflags)) {
    throw rpm_parser_exception("could not get FILEINODES header");
  }
  rpmtd_wrapper nlinks_array;
  if (!headerGet(header, RPMTAG_FILENLINKS, nlinks_array.raw, hflags)) {
    throw rpm_parser_exception("could not get FILENLINKS header");
  }
  rpmtd_wrapper digests;
  if (!headerGet(header, RPMTAG_FILEDIGESTS, digests.raw, hflags)) {
    throw rpm_parser_exception("could not get FILEDIGESTS header");
  }

  while (true) {
    const char *name = rpmtdNextString(names.raw);
    if (name == NULL) {
      break;
    }
    const uint64_t *size = rpmtdNextUint64(sizes.raw);
    if (size == NULL) {
      throw rpm_parser_exception("missing entries in LONGFILESIZES header");
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
    const uint32_t *inode = rpmtdNextUint32(inodes.raw);
    if (inode == NULL) {
      throw rpm_parser_exception("missing entries in FILEINODES header");
    }
    const uint32_t *nlinks = rpmtdNextUint32(nlinks_array.raw);
    if (nlinks == NULL) {
      throw rpm_parser_exception("missing entries in FILENLINKS header");
    }
    const char *digest = rpmtdNextString(digests.raw);
    if (digest == NULL) {
      throw rpm_parser_exception("missing entries in FILEDIGESTS header");
    }

    std::tr1::shared_ptr<rpm_file_info> p(new rpm_file_info);
    p->name = name;
    p->user = user;
    p->group = group;
    p->mtime = *mtime;
    p->mode = *mode;
    p->ino = *inode;
    p->nlinks = *nlinks;
    p->digest.set_hexadecimal(digest_algo.c_str(), *size, digest);
    files[p->name] = p;
  }

  if (rpmtdNextUint64(sizes.raw) != NULL) {
    throw rpm_parser_exception
      ("LONGFILESIZES header contains too many elements");
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

static void get_deps(Header header,
		     std::vector<rpm_dependency> &dependencies,
		     rpm_dependency::kind_type kind, bool optional,
		     int nametag, const char *namename,
		     int flagstag, const char *flagsname,
		     int versiontag, const char *versionname)
{
  const headerGetFlags hflags = HEADERGET_ALLOC | HEADERGET_EXT;
  rpmtd_wrapper name;
  rpmtd_wrapper flags;
  rpmtd_wrapper version;
  if (!headerGet(header, nametag, name.raw, hflags)) {
    if (optional) {
      return;
    }
    throw rpm_parser_exception
      ("could not get " + std::string(namename) + " header");
  }
  if (!headerGet(header, flagstag, flags.raw, hflags)) {
    throw rpm_parser_exception
      ("could not get " + std::string(flagsname) + " header");
  }
  if (!headerGet(header, versiontag, version.raw, hflags)) {
    throw rpm_parser_exception
      ("could not get " + std::string(versionname) + " header");
  }
  while (true) {
    const char *namestr = rpmtdNextString(name.raw);
    if (namestr == NULL) {
      break;
    }

    const uint32_t *flagsuint = rpmtdNextUint32(flags.raw);
    if (flagsuint == NULL) {
      throw rpm_parser_exception
	("missing entries in " + std::string(flagsname) + " header");
    }
    const char *versionstr =rpmtdNextString(version.raw);
    if (versionstr == NULL) {
      throw rpm_parser_exception
	("missing entries in " + std::string(versionname) + " header");
    }
    dependencies.push_back(rpm_dependency());
    rpm_dependency &dep = dependencies.back();
    dep.kind = kind;
    dep.capability = namestr;
    if (*flagsuint & RPMSENSE_LESS) {
      dep.op += '<';
    }
    if (*flagsuint & RPMSENSE_GREATER) {
      dep.op += '>';
    }
    if (*flagsuint & RPMSENSE_EQUAL) {
      dep.op += '=';
    }
    dep.pre = (*flagsuint & RPMSENSE_PREREQ) != 0;
    dep.version = versionstr;
  }
}

void
rpm_parser_state::impl::get_dependencies()
{
  get_deps(header, dependencies, rpm_dependency::requires, false,
	   RPMTAG_REQUIRENAME, "REQUIRENAME",
	   RPMTAG_REQUIREFLAGS, "REQUIREFLAGS",
	   RPMTAG_REQUIREVERSION, "REQUIREVERSION");
  get_deps(header, dependencies, rpm_dependency::provides, true,
	   RPMTAG_PROVIDENAME, "PROVIDENAME",
	   RPMTAG_PROVIDEFLAGS, "PROVIDEFLAGS",
	   RPMTAG_PROVIDEVERSION, "PROVIDEVERSION");
  get_deps(header, dependencies, rpm_dependency::obsoletes, true,
	   RPMTAG_OBSOLETENAME, "OBSOLETENAME",
	   RPMTAG_OBSOLETEFLAGS, "OBSOLETEFLAGS",
	   RPMTAG_OBSOLETEVERSION, "OBSOLETEVERSION");
}

void
rpm_parser_state::impl::open_payload()
{
  get_files_from_header();
  payload_is_open = true;
  const char *compr =
    headerGetString(header, RPMTAG_PAYLOADCOMPRESSOR);
  std::string rpmio_flags("r.");
  if (compr == NULL) {
    rpmio_flags += "gzip";
  } else {
    rpmio_flags += compr;
  }
  FD_t gzfd = Fdopen(fd, rpmio_flags.c_str());
  if (gzfd == NULL) {
    throw std::runtime_error("could not allocate compression handle");
  }
  if (gzfd != fd) {
    throw std::logic_error("handle changed unexpectedly");
  }
  if (Ferror(fd)) {
    throw rpm_parser_exception(Fstrerror(fd));
  }
}

rpm_parser_state::rpm_parser_state(const char *path)
  : impl_(new impl)
{
  // The code below roughly follows rpm2cpio.
  impl_->fd = Fopen(path, "r.ufdio");
  if (Ferror(impl_->fd)) {
    throw rpm_parser_exception(Fstrerror(impl_->fd));
  }

  // Load header.
  rpmts ts = rpmtsCreate();
  rpmtsSetVSFlags(ts, _RPMVSF_NOSIGNATURES);
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

  impl_->get_header();
  impl_->get_dependencies();
}

rpm_parser_state::~rpm_parser_state()
{
}

const char *
rpm_parser_state::nevra() const
{
  return rpmtdGetString(impl_->nevra.raw);
}

const rpm_package_info &
rpm_parser_state::package() const
{
  return impl_->pkg;
}

const std::vector<rpm_dependency> &
rpm_parser_state::dependencies() const
{
  return impl_->dependencies;
}

bool
rpm_parser_state::read_file(rpm_file_entry &file)
{
  if (!impl_->payload_is_open) {
    impl_->open_payload();
  }

  // Read cpio header magic.
  char cpio_magic[cpio_entry::magic_size];
  ssize_t ret = Fread(cpio_magic, sizeof(cpio_magic), 1, impl_->fd);
  if (ret == 0) {
    throw rpm_parser_exception("end of stream in cpio file header");
  } else if (ret < 0) {
    throw rpm_parser_exception(std::string(Fstrerror(impl_->fd))
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
  ret = Fread(cpio_header, cpio_len, 1, impl_->fd);
  if (ret == 0) {
    throw rpm_parser_exception("end of stream in cpio file header");
  } else if (ret < 0) {
    throw rpm_parser_exception(std::string(Fstrerror(impl_->fd))
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
  ret = Fread(name.data(), header.namesize, 1, impl_->fd);
  if (ret == 0) {
    throw rpm_parser_exception("end of stream in cpio file name");
  } else if (ret < 0) {
    throw rpm_parser_exception(std::string(Fstrerror(impl_->fd))
			       + " (in cpio file name)");
  }
  // Name padding.
  unsigned pos = 6 + cpio_len + name.size();
  while ((pos % 4) != 0) {
    char buf;
    ret = Fread(&buf, 1, 1, impl_->fd);
    if (ret == 0) {
      throw rpm_parser_exception("failed to read padding after name");
    } else if (ret < 0) {
      throw rpm_parser_exception(std::string(Fstrerror(impl_->fd))
				 + " (in cpio file name padding)");
    }
    ++pos;
  }

  // Check for end marker.
  const char *trailer = "TRAILER!!!";
  if (name.size() == strlen(trailer) + 1
      && memcmp(name.data(), trailer, strlen(trailer)) == 0) {
    return false;
  }

  // Normalize file name.
  char *name_normalized = name.data();
  if (name.size() >= 2 && name.at(0) == '.' && name.at(1) == '/') {
    ++name_normalized;
  }

  // Obtain file information.
  {
    impl::file_map::const_iterator p = impl_->files.find(name_normalized);
    if (p == impl_->files.end()) {
      throw rpm_parser_exception
	(std::string("cpio file not found in RPM header: ")
	 + name.data());
    }
    file.info = p->second;
  }
  
  // Read contents.
  file.contents.resize(header.filesize);
  if (header.filesize > 0) {
    ret = Fread(file.contents.data(), header.filesize, 1, impl_->fd);
    if (ret == 0) {
      throw rpm_parser_exception("end of stream in cpio file contents");
    } else if (ret < 0) {
      throw rpm_parser_exception(std::string(Fstrerror(impl_->fd))
				 + " (in cpio file contents)");
    }
  }

  // Contents padding.
  pos = header.filesize;
  while ((pos % 4) != 0) {
    char buf;
    ret = Fread(&buf, 1, 1, impl_->fd);
    if (ret == 0) {
      throw rpm_parser_exception("failed to read padding after cpio contents");
    } else if (ret < 0) {
      throw rpm_parser_exception(std::string(Fstrerror(impl_->fd))
				 + " (in cpio file contents padding)");
    }
    ++pos;
  }

  return true;
}

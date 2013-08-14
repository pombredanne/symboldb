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
#include <cxxll/rpmfi_handle.hpp>

#include <assert.h>
#include <limits.h>

#include <rpm/rpmlib.h>
#include <rpm/rpmlog.h>
#include <rpm/rpmpgp.h>
#include <rpm/rpmts.h>

#include <map>
#include <tr1/memory>
#include <sstream>

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
  bool reached_ghosts;

  impl()
    : fd(0), header(0), payload_is_open(false),
      reached_ghosts(false), ghost_files(files.end())
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

  rpmfi_handle fi;
  void seek_fi(uint32_t fx);

  // Pointer inside fi.
  struct findex {
    int fx_;
    bool seen_;

    explicit findex(int fx = -1)
      : fx_(fx), seen_(false)
    {
    }
  };

  typedef std::map<std::string, findex> file_map;
  file_map files;
  file_map::iterator ghost_files;

  // Records all the file indexes for an inode number.
  typedef std::multimap<uint32_t, uint32_t> hardlink_map;
  hardlink_map hardlinks;

  void get_header();
  void get_files_from_header(); // called on demand by open_payload()
  void get_dependencies(); // populate the dependencies member
  void open_payload(); // called on demand by read_file()
  bool read_file_ghost(rpm_file_entry &file);
};

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

  switch (headerIsSource(header)) {
  case 0:
    pkg.kind = rpm_package_info::binary;
    break;
  case 1:
    pkg.kind = rpm_package_info::source;
    break;
  default:
    throw rpm_parser_exception
      ("package is neither a source nor a binary package");
  }

  pkg.name = get_string(header, "NAME", RPMTAG_NAME);
  pkg.version = get_string(header, "VERSION", RPMTAG_VERSION);
  pkg.release = get_string(header, "RELEASE", RPMTAG_RELEASE);
  pkg.arch = get_string(header, "ARCH", RPMTAG_ARCH);
  pkg.hash = get_string(header, "SHA1HEADER", RPMTAG_SHA1HEADER);
  pkg.build_host = get_string(header, "BUILDHOST", RPMTAG_BUILDHOST);
  pkg.summary = get_string(header, "SUMMARY", RPMTAG_SUMMARY);
  pkg.description = get_string(header, "DESCRIPTION", RPMTAG_DESCRIPTION);
  pkg.license = get_string(header, "LICENSE", RPMTAG_LICENSE);
  pkg.group = get_string(header, "GROUP", RPMTAG_GROUP);

  {
    rpmtd_wrapper td;
    if (headerGet(header, RPMTAG_SOURCERPM, td.raw, 0)) {
      pkg.source_rpm = rpmtdGetString(td.raw);
    }
  }

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

  pkg.normalize();
}

// Returns the inode number for this hardlink.
// If not a hardlink, returns 0.
static uint32_t
hardlink_ino(rpmfi_handle &fi)
{
  if (rpmfiFNlink(fi.get()) > 1 && rpmfiWhatis(rpmfiFMode(fi.get())) == REG
      && !(rpmfiFFlags(fi.get()) & RPMFILE_GHOST)) {
    return rpmfiFInode(fi.get());
  }
  return 0;
}

void
rpm_parser_state::impl::get_files_from_header()
{
  fi.reset_header(header);
  while (fi.next()) {
    const char *name = rpmfiFN(fi.get());
    files[name] = findex(rpmfiFX(fi.get()));
    uint32_t ino = hardlink_ino(fi);
    if (ino > 0) {
      hardlinks.insert(std::make_pair(ino, rpmfiFX(fi.get())));
    }
  }
}

void
rpm_parser_state::impl::seek_fi(uint32_t fx)
{
  rpmfiSetFX(fi.get(), fx);
  // The rpmfiSetFX return value is not suitable for error checking.
  if (rpmfiFX(fi.get()) != static_cast<int>(fx)) {
    throw rpm_parser_exception("rpmfiSetFX");
  }
}

static void
get_file_info(rpmfi_handle &fi, rpm_file_info &info)
{
  info.name = rpmfiFN(fi.get());
  info.user = rpmfiFUser(fi.get());
  info.group = rpmfiFGroup(fi.get());
  info.mode = rpmfiFMode(fi.get());
  info.mtime = rpmfiFMtime(fi.get());
  info.ino = rpmfiFInode(fi.get());
  info.flags = rpmfiFFlags(fi.get());
  info.normalized = false;

  if (info.is_symlink()) {
    info.linkto = rpmfiFLink(fi.get());
  } else {
    info.linkto.clear();
  }

  int algo_rpm;
  size_t len;
  const unsigned char *digest = rpmfiFDigest(fi.get(), &algo_rpm, &len);

  switch (algo_rpm) {
  case PGPHASHALGO_MD5:
    info.digest.type = hash_sink::md5;
    break;
  case PGPHASHALGO_SHA1:
    info.digest.type = hash_sink::sha1;
    break;
  case PGPHASHALGO_SHA256:
    info.digest.type = hash_sink::sha256;
    break;
  default:
    {
      char buf[128];
      snprintf(buf, sizeof(buf), "unknown file digest algorithm %d",
	       algo_rpm);
      throw rpm_parser_exception(buf);
    }
  }
  info.digest.length = rpmfiFSize(fi.get());
  info.digest.value.clear();
  info.digest.value.insert(info.digest.value.end(), digest, digest + len);
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

bool
rpm_parser_state::impl::read_file_ghost(rpm_file_entry &file)
{
  while (ghost_files != files.end()) {
    if (!ghost_files->second.seen_) {
      file.infos.resize(1);
      rpm_file_info &info(file.infos.front());
      seek_fi(ghost_files->second.fx_);
      get_file_info(fi, info);
      if (!info.ghost()) {
	throw rpm_parser_exception("missing non-ghost file: "
				   + ghost_files->first);
      }
      file.contents.clear();
      ++ghost_files;
      return true;
    }
    ++ghost_files;
  }
  return false;
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

  // Read until we find a real entry, one that provides actual
  // contents and not just an additional name for a group of hard
  // links.
  while (true) {
    if (impl_->reached_ghosts) {
      return impl_->read_file_ghost(file);
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
      impl_->reached_ghosts = true;
      impl_->ghost_files = impl_->files.begin();
      return impl_->read_file_ghost(file);
    }

    // Normalize file name.
    char *name_normalized = name.data();
    if (name.size() >= 2 && name.at(0) == '.' && name.at(1) == '/') {
      ++name_normalized;
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

    // Obtain file information from the header.
    impl::file_map::iterator p = impl_->files.find(name_normalized);
    if (p == impl_->files.end()) {
      throw rpm_parser_exception
	(std::string("cpio file not found in RPM header: ")
	 + name.data());
    } else if (p->second.seen_) {
      throw rpm_parser_exception
	(std::string("duplicate file in CPIO archive: ")
	 + name.data());
    }

    p->second.seen_ = true;

    impl_->seek_fi(p->second.fx_);
    uint32_t ino = hardlink_ino(impl_->fi);
    if (ino == 0) {
      // This file is not a hardlink.
      file.infos.resize(1);
      get_file_info(impl_->fi, file.infos.front());
      return true;
    } else {
      // This file is hard-linked.  Check that the size matches.
      size_t fsize = rpmfiFSize(impl_->fi.get());
      if (fsize == file.contents.size()) {
	// The real file contents.  Could be empty.
	std::pair<impl::hardlink_map::iterator,
		  impl::hardlink_map::iterator> links =
	  impl_->hardlinks.equal_range(ino);
	if (links.first == links.second) {
	  throw rpm_parser_exception
	    (std::string("hard links not found: ") + name.data());
	}
	file.infos.resize(std::distance(links.first, links.second));
	size_t i = 0;
	for (impl::hardlink_map::iterator q = links.first; q != links.second; ++q) {
	  impl_->seek_fi(q->second);
	  get_file_info(impl_->fi, file.infos.at(i++));
	}
	// Check checksums for consistency.
	const checksum &ref = file.infos.front().digest;
	for (std::vector<rpm_file_info>::const_iterator
	       q = file.infos.begin() + 1, end = file.infos.end();
	     q != end; ++q) {
	  if (q->digest.value != ref.value) {
	    throw rpm_parser_exception("hard link size mismatch: " + q->name);
	  }
	  if (q->digest.value != ref.value) {
	    throw rpm_parser_exception("hard link hash mismatch: " + q->name);
	  }
	}
	return true;
      } else if (file.contents.empty()) {
	// Just another hardlink.
	continue;
      } else {
	throw rpm_parser_exception
	  (std::string("hard link size does not match header: ")
	   + name.data());
      }
    }
  }
}

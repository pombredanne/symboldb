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
#include <cxxll/rpm_file_entry.hpp>
#include <cxxll/rpm_package_info.hpp>
#include <cxxll/rpmtd_wrapper.hpp>
#include <cxxll/rpmfi_handle.hpp>
#include <cxxll/rpm_script.hpp>
#include <cxxll/rpm_trigger.hpp>
#include <cxxll/raise.hpp>

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

struct rpm_parser::impl {
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
    uint32_t fx_;
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
rpm_parser::impl::get_header()
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
      && rpmfiFSize(fi.get()) != 0
      && !(rpmfiFFlags(fi.get()) & RPMFILE_GHOST)) {
    return rpmfiFInode(fi.get());
  }
  return 0;
}

void
rpm_parser::impl::get_files_from_header()
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
rpm_parser::impl::seek_fi(uint32_t fx)
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
  info.capabilities = rpmfiFCaps(fi.get());
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

  if (info.ghost()) {
    // The size and digest from ghost files comes from the build root,
    // which is no longer available, but RPM preserves the digest in
    // some cases, particularly if hard links are involved.
    info.ino = 0;
    const char *empty =
      "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";
    info.digest.set_hexadecimal("sha256", 0, empty);
  } else {
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
    dependencies.push_back(rpm_dependency(kind));
    rpm_dependency &dep = dependencies.back();
    dep.capability = namestr;
    dep.version = versionstr;
    dep.flags = *flagsuint;
  }
}

void
rpm_parser::impl::get_dependencies()
{
  get_deps(header, dependencies, rpm_dependency::require, false,
	   RPMTAG_REQUIRENAME, "REQUIRENAME",
	   RPMTAG_REQUIREFLAGS, "REQUIREFLAGS",
	   RPMTAG_REQUIREVERSION, "REQUIREVERSION");
  get_deps(header, dependencies, rpm_dependency::provide, true,
	   RPMTAG_PROVIDENAME, "PROVIDENAME",
	   RPMTAG_PROVIDEFLAGS, "PROVIDEFLAGS",
	   RPMTAG_PROVIDEVERSION, "PROVIDEVERSION");
  get_deps(header, dependencies, rpm_dependency::obsolete, true,
	   RPMTAG_OBSOLETENAME, "OBSOLETENAME",
	   RPMTAG_OBSOLETEFLAGS, "OBSOLETEFLAGS",
	   RPMTAG_OBSOLETEVERSION, "OBSOLETEVERSION");
  get_deps(header, dependencies, rpm_dependency::conflict, true,
	   RPMTAG_CONFLICTNAME, "CONFLICTNAME",
	   RPMTAG_CONFLICTFLAGS, "CONFLICTFLAGS",
	   RPMTAG_CONFLICTVERSION, "CONFLICTVERSION");
}

void
rpm_parser::impl::open_payload()
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
    raise<std::runtime_error>("could not allocate compression handle");
  }
  if (gzfd != fd) {
    raise<std::logic_error>("handle changed unexpectedly");
  }
  if (Ferror(fd)) {
    throw rpm_parser_exception(Fstrerror(fd));
  }
}

bool
rpm_parser::impl::read_file_ghost(rpm_file_entry &file)
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

rpm_parser::rpm_parser(const char *path)
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

rpm_parser::~rpm_parser()
{
}

const char *
rpm_parser::nevra() const
{
  return rpmtdGetString(impl_->nevra.raw);
}

const rpm_package_info &
rpm_parser::package() const
{
  return impl_->pkg;
}

const std::vector<rpm_dependency> &
rpm_parser::dependencies() const
{
  return impl_->dependencies;
}

static void
get_script(Header header, std::vector<rpm_script> &result,
	   rpm_script::kind type, int scriptlettag, int progtag)
{
  const headerGetFlags hflags = HEADERGET_ALLOC | HEADERGET_EXT;
  rpmtd_wrapper scriptlet;
  rpmtd_wrapper prog;

  rpm_script script(type);

  if (headerGet(header, scriptlettag, scriptlet.raw, hflags)) {
    const char *scriptletstr = rpmtdGetString(scriptlet.raw);
    if (scriptletstr != NULL) {
      script.script = scriptletstr;
      script.script_present = true;
    }
  }
  if (headerGet(header, progtag, prog.raw, hflags)) {
    while (const char *arg = rpmtdNextString(prog.raw)) {
      script.prog.push_back(arg);
    }
  }

  if (script.script_present || !script.prog.empty()) {
    result.push_back(script);
  }
}

void
rpm_parser::scripts(std::vector<rpm_script> &result) const
{
  get_script(impl_->header, result,
	     rpm_script::pretrans, RPMTAG_PRETRANS, RPMTAG_PRETRANSPROG);
  get_script(impl_->header, result,
	     rpm_script::prein, RPMTAG_PREIN, RPMTAG_PREINPROG);
  get_script(impl_->header, result,
	     rpm_script::postin, RPMTAG_POSTIN, RPMTAG_POSTINPROG);
  get_script(impl_->header, result,
	     rpm_script::preun, RPMTAG_PREUN, RPMTAG_PREUNPROG);
  get_script(impl_->header, result,
	     rpm_script::postun, RPMTAG_POSTUN, RPMTAG_POSTUNPROG);
  get_script(impl_->header, result,
	     rpm_script::posttrans, RPMTAG_POSTTRANS, RPMTAG_POSTTRANSPROG);
  get_script(impl_->header, result,
	     rpm_script::verify, RPMTAG_VERIFYSCRIPT, RPMTAG_VERIFYSCRIPTPROG);
}

void
rpm_parser::triggers(std::vector<rpm_trigger> &result) const
{
  result.clear();

  const headerGetFlags hflags = HEADERGET_ALLOC;

  // Get the scripts.
  {
    rpmtd_wrapper scripts;
    rpmtd_wrapper progs;
    if (!headerGet(impl_->header, RPMTAG_TRIGGERSCRIPTS, scripts.raw, hflags)) {
      // No triggers, nothing to do.
      return;
    }
    if (!headerGet(impl_->header, RPMTAG_TRIGGERSCRIPTPROG, progs.raw, hflags)) {
      throw rpm_parser_exception("missing TRIGGERSCRIPTPROG header");
    }
    while (const char *script = rpmtdNextString(scripts.raw)) {
      const char *prog = rpmtdNextString(progs.raw);
      if (prog == NULL) {
	throw rpm_parser_exception("TRIGGERSCRIPTPROG array too short");
      }
      result.push_back(rpm_trigger(script, prog));
    }
  }

  // Add the conditions.
  {
    rpmtd_wrapper indexes;
    rpmtd_wrapper names;
    rpmtd_wrapper versions;
    rpmtd_wrapper flags;
    if (!headerGet(impl_->header, RPMTAG_TRIGGERINDEX, indexes.raw, hflags)) {
      // No conditions, nothing to do.
      return;
    }
    if (!headerGet(impl_->header, RPMTAG_TRIGGERNAME, names.raw, hflags)) {
      throw rpm_parser_exception("missing TRIGGERNAME header");
    }
    if (!headerGet(impl_->header, RPMTAG_TRIGGERVERSION, versions.raw, hflags)) {
      throw rpm_parser_exception("missing TRIGGERVERSION header");
    }
    if (!headerGet(impl_->header, RPMTAG_TRIGGERFLAGS, flags.raw, hflags)) {
      throw rpm_parser_exception("missing TRIGGERFLAGS header");
    }
    while (const uint32_t *idx = rpmtdNextUint32(indexes.raw)) {
      const char *name = rpmtdNextString(names.raw);
      if (name == NULL) {
	throw rpm_parser_exception("TRIGGERNAME array too short");
      }
      const char *version = rpmtdNextString(versions.raw);
      if (version == NULL) {
	throw rpm_parser_exception("TRIGGERVERSION array too short");
      }
      uint32_t *flag = rpmtdNextUint32(flags.raw);
      if (flag == NULL) {
	throw rpm_parser_exception("TRIGGERFLAGS array too short");
      }
      rpm_trigger &trigger(result.at(*idx));
      trigger.conditions.push_back(rpm_trigger::condition(name, version, *flag));
    }
  }
}

bool
rpm_parser::read_file(rpm_file_entry &file)
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
    }

    // Hardlink processing.  We use the unfiltered inode number,
    // ignoring a potential ghost state of the current CPIO entry.
    impl_->seek_fi(p->second.fx_);
    std::pair<impl::hardlink_map::iterator,
	      impl::hardlink_map::iterator> links =
      impl_->hardlinks.equal_range(rpmfiFInode(impl_->fi.get()));
    if (links.first == links.second) {
      // This file is not treated as a hardlink.
      if (p->second.seen_) {
	throw rpm_parser_exception
	  (std::string("duplicate file in CPIO archive: ")
	   + name.data());
      }
      file.infos.resize(1);
      rpm_file_info &info(file.infos.front());
      get_file_info(impl_->fi, info);
      if (info.ghost()) {
	// Bizarrely, RPM ships some ghost files with contents.
	file.contents.clear();
      }
      p->second.seen_ = true;
      return true;
    } else {
      // This file is hard-linked.  Check that the size matches.
      bool current_is_ghost =
	(rpmfiFFlags(impl_->fi.get()) & RPMFILE_GHOST) != 0;
      size_t fsize = rpmfiFSize(impl_->fi.get());
      if (fsize == file.contents.size()) {
	// We have found the real file contents.  Provide all the hard
	// links to the caller.
	file.infos.resize(std::distance(links.first, links.second));
	size_t i = 0;
	for (impl::hardlink_map::iterator q = links.first; q != links.second; ++q) {
	  impl_->seek_fi(q->second);
	  rpm_file_info &info(file.infos.at(i));
	  get_file_info(impl_->fi, info);
	  assert(!info.ghost());

	  impl::file_map::iterator p1(impl_->files.find(info.name));
	  if (p1->second.seen_) {
	    throw rpm_parser_exception
	      ("duplicate file in CPIO archive: " + info.name);
	  }
	  p1->second.seen_ = true;

	  ++i;
	}

	if (!p->second.seen_) {
	  // Ghosts are not marked as seen, so that they are reported
	  // again later on.
	  if (!current_is_ghost) {
	    throw rpm_parser_exception
	      (std::string("file not found in hard link group:")
	       + name.data());
	  }
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
	// Just another hardlink without contents.
	continue;
      } else {
	throw rpm_parser_exception
	  (std::string("hard link size does not match header: ")
	   + name.data());
      }
    }
  }
}

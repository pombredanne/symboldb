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

/*
  Part of this file has been taken from elfutils.  The original
  copyright statement follows:

  Copyright (C) 1999-2012 Red Hat, Inc.
  This file is part of elfutils.
  Written by Ulrich Drepper <drepper@redhat.com>, 1999.

  This file is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 3 of the License, or
  (at your option) any later version.

  elfutils is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#include <cxxll/elf_image.hpp>
#include <cxxll/elf_symbol_definition.hpp>
#include <cxxll/elf_symbol_reference.hpp>
#include <cxxll/elf_exception.hpp>

#include <libelf.h>
#include <gelf.h>
#include <string.h>
#include <stdio.h>

using namespace cxxll;

static const char *
get_arch(unsigned char ei_class, unsigned short e_machine)
{
  static const struct {
    unsigned short machine;
    const char *name_32;
    const char *name_64;
  } archlist[] = {
    {EM_386, "i386", NULL},
    {EM_SPARC, "sparc", NULL},
    {EM_SPARCV9, NULL, "sparc64"},
    {EM_PPC, "ppc", NULL},
    {EM_PPC64, NULL, "ppc64"},
    {EM_S390, "s390", "s390x"},
    {EM_X86_64, NULL, "x86_64"},
    {0, 0, 0}
  };
  for (unsigned i = 0; archlist[i].machine; ++i) {
    if (e_machine == archlist[i].machine) {
      if (ei_class == ELFCLASS32) {
	return archlist[i].name_32;
      } else if (ei_class == ELFCLASS64) {
	return archlist[i].name_64;
      } else {
	return NULL;
      }
    }
  }
  return NULL;
}

struct elf_image::impl {
  Elf *elf;
  size_t phnum;
  size_t shnum;
  unsigned char ei_class;
  unsigned char ei_data;
  unsigned short e_type;
  unsigned short e_machine;
  const char *arch;
  std::vector<unsigned char> build_id;

  impl(const void *start, size_t size)
    : elf(NULL)
  {
    char *p = const_cast<char *>(static_cast<const char *>(start));
    elf = elf_memory(p, size);
    if (elf == NULL) {
      throw elf_exception();
    }

    GElf_Ehdr ehdr_mem;
    GElf_Ehdr *ehdr = gelf_getehdr (elf, &ehdr_mem);
    if (ehdr == NULL) {
      elf_exception::raise("cannot read ELF header: %s", elf_errmsg (-1));
    }
    ei_class = ehdr->e_ident[EI_CLASS];
    ei_data = ehdr->e_ident[EI_DATA];
    e_type = ehdr->e_type;
    e_machine = ehdr->e_machine;
    arch = get_arch(ei_class, e_machine);

    if (elf_getshdrnum (elf, &shnum) < 0) {
      throw elf_exception();
    }
    if (elf_getphdrnum (elf, &phnum) < 0) {
      throw elf_exception();
    }

    set_build_id();
  }

  ~impl()
  {
    elf_end(elf);
  }

  void set_build_id();
  void set_build_id_from_section(Elf_Data *data);
};

void
elf_image::impl::set_build_id()
{
  // See handle_notes() in readelf.c (from elfutils).
  if (shnum == 0) {
    Elf_Scn *scn = NULL;
    while (true) {
      scn = elf_nextscn(elf, scn);
      if (scn == NULL) {
	break;
      }
      GElf_Shdr shdr_mem;
      GElf_Shdr *shdr = gelf_getshdr(scn, &shdr_mem);
      if (shdr == NULL || shdr->sh_type != SHT_NOTE
	  || (shdr->sh_flags & SHF_ALLOC) == 0) {
	continue;
      }

      Elf_Data *data = elf_getdata (scn, NULL);
      if (data != NULL) {
	set_build_id_from_section(data);
	return;
      }
    }
    return;
  }

  // Look at the program header if no section headers are present.
  for (size_t i = 0; i < phnum; ++i) {
    GElf_Phdr mem;
    GElf_Phdr *phdr = gelf_getphdr (elf, i, &mem);
    if (phdr == NULL || phdr->p_type != PT_NOTE) {
      continue;
    }
    Elf_Data *data =
      elf_getdata_rawchunk (elf, phdr->p_offset, phdr->p_filesz, ELF_T_NHDR);
    if (data != NULL) {
      set_build_id_from_section(data);
      return;
    }
  }
}

void
elf_image::impl::set_build_id_from_section(Elf_Data *data)
{
  // See handle_notes_data() in readelf.c (from elfutils).
  size_t offset = 0;
  GElf_Nhdr nhdr;
  size_t name_offset;
  size_t desc_offset;
  while (offset < data->d_size) {
    offset = gelf_getnote(data, offset, &nhdr, &name_offset, &desc_offset);
    if (offset == 0) {
      break;
    }
    const char *name = static_cast<const char *>(data->d_buf) + name_offset;
    if (strcmp(name, "GNU") == 0 && nhdr.n_type == 3) {
      const unsigned char *desc =
	static_cast<unsigned char *>(data->d_buf) + desc_offset;
      build_id.assign(desc, desc + nhdr.n_descsz);
      return;
    }
  }
}

void
cxxll::elf_image_init()
{
  elf_version(EV_CURRENT);
}

elf_image::elf_image(const void *start, size_t size)
  : impl_(new impl(start, size))
{
}

elf_image::~elf_image()
{
}

unsigned char
elf_image::ei_class() const
{
  return impl_->ei_class;
}

unsigned char
elf_image::ei_data() const
{
  return impl_->ei_data;
}

unsigned short
elf_image::e_type() const
{
  return impl_->e_type;
}

unsigned short
elf_image::e_machine() const
{
  return impl_->e_machine;
}

const char *
elf_image::arch() const
{
  return impl_->arch;
}

const std::vector<unsigned char> &
elf_image::build_id() const
{
  return impl_->build_id;
}

struct elf_image::symbol_range::state {
  // The following are set up by next_section().
  bool eof;
  Elf_Scn *scn;
  GElf_Shdr *shdr;
  GElf_Shdr shdr_mem;
  
  // The following are set up by init_section().
  unsigned nsyms;
  Elf_Data *data;
  Elf_Data *xndx_data;
  Elf_Data *versym_data;
  Elf_Data *verneed_data;
  Elf_Data *verdef_data;
  Elf32_Word verneed_stridx;
  Elf32_Word verdef_stridx;

  // These are updated by next().
  unsigned cnt;
  std::tr1::shared_ptr<elf_symbol_definition> def;
  std::tr1::shared_ptr<elf_symbol_reference> ref;

  state()
    : eof(false), scn(NULL), nsyms(0), cnt(0)
  {
  }

  bool next(impl *parent);
  bool next_section(impl *parent);
  bool init_section(impl *parent);
};

bool
elf_image::symbol_range::state::next_section(impl *parent)
{
  if (eof) {
    return false;
  }

  while ((scn = elf_nextscn (parent->elf, scn)) != NULL) {
    shdr = gelf_getshdr (scn, &shdr_mem);
    if (shdr != NULL && (shdr->sh_type == (GElf_Word) SHT_DYNSYM 
			 || shdr->sh_type == (GElf_Word) SHT_SYMTAB)) {
      if (init_section(parent)) {
	return true;
      }
    }
  }
  eof = true;
  return false;
}

bool
elf_image::symbol_range::state::init_section(impl *parent)
{
  int class_ = gelf_getclass (parent->elf);
  xndx_data = NULL;
  versym_data = NULL;
  verneed_data = NULL;
  verdef_data = NULL;
  verneed_stridx = 0;
  verdef_stridx = 0;

  /* Get the data of the section.  */
  data = elf_getdata (scn, NULL);
  if (data == NULL)
    return false;

  /* Find out whether we have other sections we might need.  */
  Elf_Scn *runscn = NULL;
  while ((runscn = elf_nextscn (parent->elf, runscn)) != NULL)
    {
      GElf_Shdr runshdr_mem;
      GElf_Shdr *runshdr = gelf_getshdr (runscn, &runshdr_mem);

      if (runshdr != NULL)
	{
	  if (runshdr->sh_type == SHT_GNU_versym
	      && runshdr->sh_link == elf_ndxscn (scn))
	    /* Bingo, found the version information.  Now get the data.  */
	    versym_data = elf_getdata (runscn, NULL);
	  else if (runshdr->sh_type == SHT_GNU_verneed)
	    {
	      /* This is the information about the needed versions.  */
	      verneed_data = elf_getdata (runscn, NULL);
	      verneed_stridx = runshdr->sh_link;
	    }
	  else if (runshdr->sh_type == SHT_GNU_verdef)
	    {
	      /* This is the information about the defined versions.  */
	      verdef_data = elf_getdata (runscn, NULL);
	      verdef_stridx = runshdr->sh_link;
	    }
	  else if (runshdr->sh_type == SHT_SYMTAB_SHNDX
	      && runshdr->sh_link == elf_ndxscn (scn))
	    /* Extended section index.  */
	    xndx_data = elf_getdata (runscn, NULL);
	}
    }

  /* Get the section header string table index.  */
  size_t shstrndx;
  if (elf_getshdrstrndx (parent->elf, &shstrndx) < 0)
    throw elf_exception("cannot get section header string table index");

  GElf_Shdr glink_mem;
  GElf_Shdr *glink = gelf_getshdr (elf_getscn (parent->elf, shdr->sh_link),
				   &glink_mem);
  if (glink == NULL)
    elf_exception::raise("invalid sh_link value in section %zu",
				  elf_ndxscn (scn));

  /* Now we can compute the number of entries in the section.  */
  nsyms = data->d_size / (class_ == ELFCLASS32
			  ? sizeof (Elf32_Sym)
			  : sizeof (Elf64_Sym));
  cnt = 0;
  return true;
}

bool
elf_image::symbol_range::state::next(impl *parent)
{
  def.reset();
  ref.reset();

  GElf_Sym sym_mem;
  GElf_Sym *sym;
  Elf32_Word xndx;

  while (true) {
    if (cnt >= nsyms) {
      if (!next_section(parent)) {
	return false;
      }
    }
    sym = gelf_getsymshndx (data, xndx_data, cnt, &sym_mem, &xndx);
    if (sym != NULL) {
      break;
    }
    ++cnt;
  }

  /* Determine the real section index.  */
  if (sym->st_shndx != SHN_XINDEX)
    xndx = sym->st_shndx;
  bool check_def = xndx != SHN_UNDEF;

  elf_symbol_definition def_new;
  elf_symbol_reference ref_new;
  if (versym_data != NULL)
    {
      /* Get the version information.  */
      GElf_Versym versym_mem;
      GElf_Versym *versym = gelf_getversym (versym_data, cnt, &versym_mem);

      if (versym != NULL && ((*versym & 0x8000) != 0 || *versym > 1))
	{
	  bool is_nobits = false;

	  if (xndx < SHN_LORESERVE || sym->st_shndx == SHN_XINDEX)
	    {
	      GElf_Shdr symshdr_mem;
	      GElf_Shdr *symshdr =
		gelf_getshdr (elf_getscn (parent->elf, xndx), &symshdr_mem);

	      is_nobits = (symshdr != NULL
			   && symshdr->sh_type == SHT_NOBITS);
	    }

	  if (is_nobits || ! check_def)
	    {
	      /* We must test both.  */
	      GElf_Vernaux vernaux_mem;
	      GElf_Vernaux *vernaux = NULL;
	      size_t vn_offset = 0;

	      GElf_Verneed verneed_mem;
	      GElf_Verneed *verneed = gelf_getverneed (verneed_data, 0,
						       &verneed_mem);
	      while (verneed != NULL)
		{
		  size_t vna_offset = vn_offset;

		  vernaux = gelf_getvernaux (verneed_data,
					     vna_offset += verneed->vn_aux,
					     &vernaux_mem);
		  while (vernaux != NULL
			 && vernaux->vna_other != *versym
			 && vernaux->vna_next != 0)
		    {
		      /* Update the offset.  */
		      vna_offset += vernaux->vna_next;

		      vernaux = (vernaux->vna_next == 0
				 ? NULL
				 : gelf_getvernaux (verneed_data,
						    vna_offset,
						    &vernaux_mem));
		    }

		  /* Check whether we found the version.  */
		  if (vernaux != NULL && vernaux->vna_other == *versym)
		    /* Found it.  */
		    break;

		  vn_offset += verneed->vn_next;
		  verneed = (verneed->vn_next == 0
			     ? NULL
			     : gelf_getverneed (verneed_data, vn_offset,
						&verneed_mem));
		}

	      if (vernaux != NULL && vernaux->vna_other == *versym)
		{
		  ref_new.vna_name =
		    elf_strptr (parent->elf, verneed_stridx,
				vernaux->vna_name);
		  ref_new.vna_other = vernaux->vna_other;
		  check_def = 0;
		}
	      else if (! is_nobits)
		elf_exception::raise("bad dynamic symbol %u", cnt);
	      else
		check_def = 1;
	    }

	  if (check_def && *versym != 0x8001)
	    {
	      /* We must test both.  */
	      size_t vd_offset = 0;

	      GElf_Verdef verdef_mem;
	      GElf_Verdef *verdef = gelf_getverdef (verdef_data, 0,
						    &verdef_mem);
	      while (verdef != NULL)
		{
		  if (verdef->vd_ndx == (*versym & 0x7fff))
		    /* Found the definition.  */
		    break;

		  vd_offset += verdef->vd_next;
		  verdef = (verdef->vd_next == 0
			    ? NULL
			    : gelf_getverdef (verdef_data, vd_offset,
					      &verdef_mem));
		}

	      if (verdef != NULL)
		{
		  GElf_Verdaux verdaux_mem;
		  GElf_Verdaux *verdaux
		    = gelf_getverdaux (verdef_data,
				       vd_offset + verdef->vd_aux,
				       &verdaux_mem);
		      
		  if (verdaux != NULL)
		    def_new.vda_name =
		      elf_strptr (parent->elf, verdef_stridx,
				  verdaux->vda_name);
		  if (*versym & 0x8000)
		    def_new.default_version = true;
		  // FIXME: printf ((*versym & 0x8000) ? "@%s" : "@@%s",
		  // FIXME: clarify @/@@ difference
		}
	    }
	}
    }

  elf_symbol *psymbol;
  if (check_def) {
    def_new.section = sym->st_shndx;
    def_new.xsection = xndx;
    def.reset(new elf_symbol_definition(def_new));
    psymbol = def.get();
  } else {
    ref.reset(new elf_symbol_reference(ref_new));
    psymbol = ref.get();
  }
  psymbol->type = GELF_ST_TYPE (sym->st_info);
  psymbol->binding = GELF_ST_BIND (sym->st_info);
  psymbol->symbol_name =
    elf_strptr (parent->elf, shdr->sh_link, sym->st_name);
  psymbol->other = sym->st_other;
  ++cnt;
  return true;
}

elf_image::symbol_range::symbol_range(const elf_image &image)
  : impl_(image.impl_), state_(new state)
{
}

elf_image::symbol_range::~symbol_range()
{
}

bool
elf_image::symbol_range::next()
{
  return state_->next(impl_.get());
}

std::tr1::shared_ptr<elf_symbol_definition>
elf_image::symbol_range::definition() const
{
  return state_->def;
}

std::tr1::shared_ptr<elf_symbol_reference>
elf_image::symbol_range::reference() const
{
  return state_->ref;
}

struct elf_image::dynamic_section_range::state {
  std::tr1::shared_ptr<impl> impl_;
  GElf_Shdr *shdr;
  Elf_Data *data;
  size_t cnt;
  size_t entries;
  GElf_Shdr shdr_mem;
  std::string value;
  kind type;

  state(const elf_image &image)
    : impl_(image.impl_), cnt(0), entries(0)
  {
    for (size_t i = 0; i < impl_->phnum; ++i) {
      GElf_Phdr phdr_mem;
      GElf_Phdr *phdr = gelf_getphdr (impl_->elf, i, &phdr_mem);
      if (phdr != NULL && phdr->p_type == PT_DYNAMIC) {
	Elf_Scn *scn = gelf_offscn (impl_->elf, phdr->p_offset);
	shdr = gelf_getshdr (scn, &shdr_mem);
	if (shdr != NULL && shdr->sh_type == SHT_DYNAMIC) {
	  // Section found, use it for processing if not empty.
	  data = elf_getdata (scn, NULL);
	  if (data != NULL) {
	    entries = shdr->sh_size / shdr->sh_entsize;
	  }
	  break;
	} else {
	  shdr = NULL;
	}
      }
    }

  }

  bool next()
  {
    value.clear();
    while (cnt < entries) {
      GElf_Dyn dynmem;
      GElf_Dyn *dyn = gelf_getdyn (data, cnt, &dynmem);
      ++cnt;
      switch (dyn->d_tag) {
      case DT_NEEDED:
	type = needed;
	break;
      case DT_SONAME:
	type = soname;
	break;
      case DT_RPATH:
	type = rpath;
	break;
      case DT_RUNPATH:
	type = runpath;
	break;
      default:
	// Nothing useful found, try again.
	continue;
      }
      value = elf_strptr (impl_->elf, shdr->sh_link, dyn->d_un.d_val);
      return true;
    }
    return false;
  }
};


elf_image::dynamic_section_range::dynamic_section_range(const elf_image &image)
  : state_(new state(image))
{
}

elf_image::dynamic_section_range::~dynamic_section_range()
{
}

bool
elf_image::dynamic_section_range::next()
{
  return state_->next();
}

elf_image::dynamic_section_range::kind
elf_image::dynamic_section_range::type() const
{
  return state_->type;
}

const std::string &
elf_image::dynamic_section_range::value() const
{
  return state_->value;
}

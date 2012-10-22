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

#include "elf_image.hpp"
#include "elf_symbol_definition.hpp"
#include "elf_symbol_reference.hpp"
#include "elf_exception.hpp"

#include <libelf.h>
#include <elfutils/libebl.h>

struct elf_image::impl {
  Elf *elf;
  Ebl *ebl;
  size_t shnum;

  impl(const void *start, size_t size)
    : elf(NULL), ebl(NULL)
  {
    char *p = const_cast<char *>(static_cast<const char *>(start));
    elf = elf_memory(p, size);
    if (elf == NULL) {
      throw elf_exception();
    }
    ebl = ebl_openbackend (elf);
    if (ebl == NULL) {
      elf_exception::raise("cannot create EBL handle");
    }
  }

  ~impl()
  {
    ebl_closebackend(ebl);
    elf_end(elf);
  }
};


void
elf_image_init()
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

struct elf_image::symbol_range_impl : elf_image::symbol_range {
  std::tr1::shared_ptr<elf_image::impl> parent;

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

  symbol_range_impl(const elf_image &e)
    : parent(e.impl_), eof(false), scn(NULL), nsyms(0), cnt(0)
  {
  }

  bool next();
  bool next_section();
  bool init_section();
  std::tr1::shared_ptr<elf_symbol_definition> definition() const;
  std::tr1::shared_ptr<elf_symbol_reference> reference() const;
};

bool
elf_image::symbol_range_impl::next_section()
{
  if (eof) {
    return false;
  }

  while ((scn = elf_nextscn (parent->elf, scn)) != NULL) {
    shdr = gelf_getshdr (scn, &shdr_mem);
    if (shdr != NULL && (shdr->sh_type == (GElf_Word) SHT_DYNSYM 
			 || shdr->sh_type == (GElf_Word) SHT_SYMTAB)) {
      if (init_section()) {
	return true;
      }
    }
  }
  eof = true;
  return false;
}

bool
elf_image::symbol_range_impl::init_section()
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
    elf_exception::raise("invalid sh_link value in section %Zu",
				  elf_ndxscn (scn));

  /* Now we can compute the number of entries in the section.  */
  nsyms = data->d_size / (class_ == ELFCLASS32
			  ? sizeof (Elf32_Sym)
			  : sizeof (Elf64_Sym));
  cnt = 0;
  return true;
}

static const char *
get_visibility_type (int value)
{
  switch (value)
    {
    case STV_DEFAULT:
      return "DEFAULT";
    case STV_INTERNAL:
      return "INTERNAL";
    case STV_HIDDEN:
      return "HIDDEN";
    case STV_PROTECTED:
      return "PROTECTED";
    default:
      return "???";
    }
}

bool
elf_image::symbol_range_impl::next()
{
  def.reset();
  ref.reset();

  GElf_Sym sym_mem;
  GElf_Sym *sym;
  Elf32_Word xndx;

  while (true) {
    if (cnt >= nsyms) {
      if (!next_section()) {
	return false;
      }
    }
    sym = gelf_getsymshndx (data, xndx_data, cnt, &sym_mem, &xndx);
    if (sym != NULL) {
      break;
    }
    ++cnt;
  }

  char typebuf[64];
  char bindbuf[64];
  char scnbuf[64];

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
    def_new.section_name = 
      ebl_section_name (parent->ebl, sym->st_shndx, xndx, scnbuf,
			sizeof (scnbuf), NULL, parent->shnum);
    def.reset(new elf_symbol_definition(def_new));
    psymbol = def.get();
  } else {
    ref.reset(new elf_symbol_reference(ref_new));
    psymbol = ref.get();
  }
  psymbol->type_name =
    ebl_symbol_type_name (parent->ebl, GELF_ST_TYPE (sym->st_info),
			  typebuf, sizeof (typebuf));
  psymbol->binding_name =
    ebl_symbol_binding_name (parent->ebl, GELF_ST_BIND (sym->st_info),
			     bindbuf, sizeof (bindbuf));
  psymbol->visibility_type =
    get_visibility_type (GELF_ST_VISIBILITY (sym->st_other));
  psymbol->symbol_name =
    elf_strptr (parent->elf, shdr->sh_link, sym->st_name);
  ++cnt;
  return true;
}

std::tr1::shared_ptr<elf_symbol_definition>
elf_image::symbol_range_impl::definition() const
{
  return def;
}

std::tr1::shared_ptr<elf_symbol_reference>
elf_image::symbol_range_impl::reference() const
{
  return ref;
}


std::tr1::shared_ptr<elf_image::symbol_range>
elf_image::symbols()
{
  return std::tr1::shared_ptr<symbol_range>(new symbol_range_impl(*this));
}

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

#include "find-symbols.hpp"
#include "elf_symbol_definition.hpp"
#include "elf_symbol_reference.hpp"

#include <elfutils/libebl.h>

namespace {

  struct impl {
    Elf *elf;
    Ebl *ebl;
    size_t shnum;
    find_symbols_callbacks callbacks;

    impl(Elf *, const find_symbols_callbacks &);
    ~impl();
    void process_sections_by_type(int type);
    void process_section(Elf_Scn *scn, GElf_Shdr *shdr);
  };
}

void find_symbols(Elf *elf, const find_symbols_callbacks &callbacks)
{
  impl i(elf, callbacks);
  i.process_sections_by_type(SHT_DYNSYM);
  i.process_sections_by_type(SHT_SYMTAB);
}


static const char * get_visibility_type (int value);

impl::impl(Elf *elf, const find_symbols_callbacks &callbacks)
{
  this->elf = elf;
  if (elf_getshdrnum (elf, &shnum) < 0) {
    find_symbols_exception::raise
      ("cannot determine number of sections: %s", elf_errmsg (-1));
  }
  ebl = ebl_openbackend (elf);
  if (ebl == NULL) {
    find_symbols_exception::raise("cannot create EBL handle");
  }
  this->callbacks = callbacks;
}

impl::~impl()
{
  ebl_closebackend(ebl);
}

void
impl::process_sections_by_type(int type)
{
  /* Find the symbol table(s).  For this we have to search through the
     section table.  */
  Elf_Scn *scn = NULL;

  while ((scn = elf_nextscn (elf, scn)) != NULL)
    {
      /* Handle the section if it is a symbol table.  */
      GElf_Shdr shdr_mem;
      GElf_Shdr *shdr = gelf_getshdr (scn, &shdr_mem);

      if (shdr != NULL && shdr->sh_type == (GElf_Word) type)
	process_section (scn, shdr);
    }
}

void
impl::process_section(Elf_Scn *scn, GElf_Shdr *shdr)
{
  Elf_Data *versym_data = NULL;
  Elf_Data *verneed_data = NULL;
  Elf_Data *verdef_data = NULL;
  Elf_Data *xndx_data = NULL;
  int class_ = gelf_getclass (elf);
  Elf32_Word verneed_stridx = 0;
  Elf32_Word verdef_stridx = 0;

  /* Get the data of the section.  */
  Elf_Data *data = elf_getdata (scn, NULL);
  if (data == NULL)
    return;

  /* Find out whether we have other sections we might need.  */
  Elf_Scn *runscn = NULL;
  while ((runscn = elf_nextscn (elf, runscn)) != NULL)
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
  if (elf_getshdrstrndx (elf, &shstrndx) < 0)
    throw find_symbols_exception("cannot get section header string table index");

  GElf_Shdr glink_mem;
  GElf_Shdr *glink = gelf_getshdr (elf_getscn (elf, shdr->sh_link),
				   &glink_mem);
  if (glink == NULL)
    find_symbols_exception::raise("invalid sh_link value in section %Zu",
				  elf_ndxscn (scn));

  /* Now we can compute the number of entries in the section.  */
  unsigned int nsyms = data->d_size / (class_ == ELFCLASS32
				       ? sizeof (Elf32_Sym)
				       : sizeof (Elf64_Sym));

  for (unsigned int cnt = 0; cnt < nsyms; ++cnt)
    {
      char typebuf[64];
      char bindbuf[64];
      char scnbuf[64];
      Elf32_Word xndx;
      GElf_Sym sym_mem;
      GElf_Sym *sym = gelf_getsymshndx (data, xndx_data, cnt, &sym_mem, &xndx);

      if (sym == NULL)
	continue;

      /* Determine the real section index.  */
      if (sym->st_shndx != SHN_XINDEX)
	xndx = sym->st_shndx;
      bool check_def = xndx != SHN_UNDEF;

      elf_symbol_definition dsinfo;
      elf_symbol_reference usinfo;
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
		    gelf_getshdr (elf_getscn (elf, xndx), &symshdr_mem);

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
		      usinfo.vna_name = elf_strptr (elf, verneed_stridx,
						    vernaux->vna_name);
		      usinfo.vna_other = vernaux->vna_other;
		      check_def = 0;
		    }
		  else if (! is_nobits)
		    find_symbols_exception::raise
		      ("bad dynamic symbol %u", cnt);
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
			dsinfo.vda_name = elf_strptr (elf, verdef_stridx,
						      verdaux->vda_name);
		      if (*versym & 0x8000)
			dsinfo.default_version = true;
		      // FIXME: printf ((*versym & 0x8000) ? "@%s" : "@@%s",
		      // FIXME: clarify @/@@ difference
		    }
		}
	    }
	}

      elf_symbol *psinfo;
      if (check_def) {
	psinfo = &dsinfo;
	dsinfo.section_name = 
	  ebl_section_name (ebl, sym->st_shndx, xndx, scnbuf,
			    sizeof (scnbuf), NULL, shnum);
      } else {
	psinfo = &usinfo;
      }
      psinfo->type_name =
	ebl_symbol_type_name (ebl, GELF_ST_TYPE (sym->st_info),
			      typebuf, sizeof (typebuf));
      psinfo->binding_name =
	ebl_symbol_binding_name (ebl, GELF_ST_BIND (sym->st_info),
				 bindbuf, sizeof (bindbuf));
      psinfo->visibility_type =
	get_visibility_type (GELF_ST_VISIBILITY (sym->st_other));
      psinfo->symbol_name = elf_strptr (elf, shdr->sh_link, sym->st_name);
      if (check_def) {
	callbacks.definition(dsinfo);
      } else {
	callbacks.reference(usinfo);
      }
    }
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

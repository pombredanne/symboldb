/*
 * Copyright (C) 2012 Red Hat, Inc.
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

#pragma once

#include <tr1/memory>

class elf_symbol_definition;
class elf_symbol_reference;

class elf_image {
  struct impl;
  std::tr1::shared_ptr<impl> impl_;
  struct symbol_range_impl;
public:
  // Opens an in-memory elf image.
  elf_image(const void *, size_t);
  ~elf_image();

  // Fields from the ELF header.
  unsigned char ei_class() const;
  unsigned char ei_data() const;
  unsigned short e_type() const;
  unsigned short e_machine() const;

  // Architecture from the e_machine field, in RPM-compatible format.
  // Can be NULL if the architecture cannot be determined.
  const char *arch() const;

  class symbol_range {
    std::tr1::shared_ptr<impl> impl_;
    struct state;
    std::tr1::shared_ptr<state> state_;
  public:
    symbol_range(const elf_image &);
    ~symbol_range();

    // Advances to the next symbol.
    bool next();

    // These return a null pointer if the iterator is not position at
    // information of this kind.
    std::tr1::shared_ptr<elf_symbol_definition> definition() const;
    std::tr1::shared_ptr<elf_symbol_reference> reference() const;
  };

  // Iterates over strings in the dynamic section.
  class dynamic_section_range {
    struct state;
    std::tr1::shared_ptr<state> state_;
  public:
    dynamic_section_range(const elf_image &);
    ~dynamic_section_range();

    // Advances to the next entry.
    bool next();

    typedef enum {
      needed, soname, rpath, runpath
    } kind;

    kind type() const;
    const std::string &value() const;
  };
};

// Call this to initialize the subsystem.
void elf_image_init();

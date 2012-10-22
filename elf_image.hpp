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

  struct symbol_range {
    virtual ~symbol_range() { }

    // Advances to the next symbol.
    virtual bool next() = 0;

    // These return a null pointer if the iterator is not position at
    // information of this kind.
    virtual std::tr1::shared_ptr<elf_symbol_definition> definition() const = 0;
    virtual std::tr1::shared_ptr<elf_symbol_reference> reference() const = 0;
  };

  std::tr1::shared_ptr<symbol_range> symbols();
};

// Call this to initialize the subsystem.
void elf_image_init();

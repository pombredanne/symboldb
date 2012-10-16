#pragma once

#include "cpio_reader.hpp"

#include <stdexcept>
#include <tr1/memory>
#include <string>
#include <vector>

// This needs to be called once before creating any rpm_parser_state
// objects.
void rpm_parser_init();

// Thrown if constructing an rpm_parser_state object fails.
struct rpm_parser_exception : std::runtime_error {
  rpm_parser_exception(const std::string &);
  rpm_parser_exception(const char *);
};

class rpm_parser_state {
  struct impl;
  std::tr1::shared_ptr<impl> impl_;
public:
  // Opens the RPM file at PATH.
  rpm_parser_state(const char *path);
  ~rpm_parser_state();

  // Reads the next payload entry.  Returns true if an entry has been
  // read, false on EOF.  Throws rpm_parser_exception on read errors.
  bool read_file(cpio_entry &entry,
		 std::vector<char> &name,
		 std::vector<char> &contents);
};

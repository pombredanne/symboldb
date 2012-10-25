#pragma once

#include <string>
#include <stdexcept>

// Thrown if constructing an rpm_parser_state object fails.
struct rpm_parser_exception : std::runtime_error {
  rpm_parser_exception(const std::string &);
  rpm_parser_exception(const char *);
};

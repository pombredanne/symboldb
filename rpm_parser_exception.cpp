#include "rpm_parser.hpp"

rpm_parser_exception::rpm_parser_exception(const char *msg)
  : std::runtime_error(msg)
{
}

rpm_parser_exception::rpm_parser_exception(const std::string &msg)
  : std::runtime_error(msg)
{
}

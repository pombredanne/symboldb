#include "database.hpp"

database_exception::database_exception(const char *msg)
  : std::runtime_error(msg)
{
}

database_exception::database_exception(const std::string &msg)
  : std::runtime_error(msg)
{
}


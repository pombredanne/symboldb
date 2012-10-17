#pragma once

#include <stdint.h>
#include <sys/types.h>

#include <string>

// Information about a file in an RPM.
// This data comes from the RPM header, not the cpio section.
struct rpm_file_info {
  std::string name;
  std::string user;
  std::string group;
  uint32_t mode;
  uint32_t mtime;

  rpm_file_info();
  ~rpm_file_info();
};

// TODO:
// symlinks, hardlinks?
// non-regular files?
// file-system capabilities?

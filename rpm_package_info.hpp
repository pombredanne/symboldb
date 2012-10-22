#pragma once

#include <string>

// Information about an entire RPM package.
struct rpm_package_info {
  std::string name;
  std::string version;
  std::string release;
  std::string arch;
  std::string hash;		// 40 hexadecimal characters
  int epoch;			// -1 if no epoch

  rpm_package_info();
  ~rpm_package_info();
};

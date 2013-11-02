/*
 * Copyright (C) 2013 Red Hat, Inc.
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

#include <memory>
#include <vector>
#include <map>

#include <cxxll/rpm_evr.hpp>
#include <cxxll/rpm_package_info.hpp>

namespace cxxll {

// Picks the largest version number for each package name/architecture
// combinations.
template <class T>
class package_set_consolidator {
  struct key {
    std::string name;
    std::string arch;

    bool operator<(const key &other) const;
  };

  struct value {
    rpm_evr version;
    T data;

    value(const rpm_evr &ver, const T &val)
      : version(ver), data(val)
    {
    }
  };

  typedef std::map<std::string, value> name_map;
  typedef std::map<std::string, name_map> arch_map;
  arch_map map;

public:
  package_set_consolidator();
  ~package_set_consolidator();

  void add(const rpm_package_info &, const T &value);
  std::vector<T> values() const;
};

template <class T>
package_set_consolidator<T>::package_set_consolidator()
{
}

template <class T>
package_set_consolidator<T>::~package_set_consolidator()
{
}

template <class T>
void
package_set_consolidator<T>::add(const rpm_package_info &info, const T &v)
{
  rpm_evr evr;
  evr.epoch = info.epoch;
  evr.version = info.version;
  evr.release = info.release;


  name_map &nmap(map[info.arch]);
  typename name_map::iterator p(nmap.find(info.name));
  if (p == nmap.end()) {
    nmap.insert(std::make_pair(info.name, value(evr, v)));
  } else {
    if (p->second.version < evr) {
      p->second.version = evr;
      p->second.data = v;
    }
  }
}

template <class T>
std::vector<T>
package_set_consolidator<T>::values() const
{
  std::vector<T> result;
  for (const std::pair<std::string, name_map> &archp : map) {
    for (const std::pair<std::string, value> &namep : archp.second) {
      result.push_back(namep.second.data);
    }
  }
  return result;
}

} // namespace cxxll

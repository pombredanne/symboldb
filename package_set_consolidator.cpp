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

#include "package_set_consolidator.hpp"
#include "rpm_package_info.hpp"

#include <map>

#include <rpm/rpmlib.h>

struct package_set_consolidator::impl {
  struct key {
    std::string name;
    std::string arch;

    bool operator<(const key &other) const;
  };

  struct evr {
    std::string epoch;
    std::string version;
    std::string release;

    bool operator<(const evr &other) const;
  };

  struct value {
    evr version;
    database::package_id id;

    value() 
      : id(0)
    {
    }
  };

  typedef std::map<std::string, value> name_map;
  typedef std::map<std::string, name_map> arch_map;

  arch_map map;
};

bool
package_set_consolidator::impl::evr::operator<(const evr &other) const
{
  if (epoch == other.epoch) {
    int ret = rpmvercmp(version.c_str(), other.version.c_str());
    if (ret != 0) {
      return ret < 0;
    }
    ret = rpmvercmp(release.c_str(), other.release.c_str());
    return ret < 0;
  }
  if (epoch.size() != other.epoch.size()) {
    return epoch.size() < other.epoch.size();
  }
  return epoch < other.epoch;
}

package_set_consolidator::package_set_consolidator()
  : impl_(new impl)
{
}

package_set_consolidator::~package_set_consolidator()
{
}

void
package_set_consolidator::add(const rpm_package_info &info,
			      database::package_id id)
{
  impl::evr evr;
  evr.epoch = info.epoch;
  evr.version = info.version;
  evr.release = info.release;

  impl::value &value = impl_->map[info.arch][info.name];
  if (value.id == 0 || value.version < evr) {
    value.version = evr;
    value.id = id;
  }
}

std::vector<database::package_id>
package_set_consolidator::package_ids() const
{
  std::vector<database::package_id> result;
  for (impl::arch_map::const_iterator archp = impl_->map.begin(),
	 archend = impl_->map.end(); archp != archend; ++archp) {
    const impl::name_map &arch = archp->second;
    for (impl::name_map::const_iterator namep = arch.begin(),
	   nameend = arch.end(); namep != nameend; ++namep) {
      result.push_back(namep->second.id);
    }
  }
  return result;
}

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

#include "rpm_evr.hpp"

#include <rpm/rpmlib.h>

rpm_evr::rpm_evr()
{
}

rpm_evr::~rpm_evr()
{
}

bool
rpm_evr::operator<(const rpm_evr &other) const
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


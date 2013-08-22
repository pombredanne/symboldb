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

#include <string>
#include <vector>

namespace cxxll {
  class expat_source;

  struct maven_url {
    std::string url;

    enum kind {
      other,
      repository,
      pluginRepository,
      snapshotRepository,
      distributionManagement,
      downloadUrl, // inside distributionManagement
      site, // inside distributionManagement
      scm,
      connection, // inside scm
      developerConnection, // inside scm
    } type;

    maven_url();
    maven_url(const char *, kind);

    // Tries to extract Maven URLs from an XML document.  The data is
    // added to RESULT.
    static void extract(expat_source &, std::vector<maven_url> &result);

    static const char *to_string(kind);
  };
}

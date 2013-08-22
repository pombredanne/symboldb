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

#include <cxxll/maven_url.hpp>
#include <cxxll/expat_source.hpp>
#include <cxxll/source.hpp>
#include <cxxll/fd_handle.hpp>
#include <cxxll/fd_source.hpp>

#include <tr1/memory>

#include "test.hpp"

using namespace cxxll;

namespace {
  struct open_xml {
    std::string path_;
    std::vector<maven_url> urls;

    explicit open_xml(const char *path);
    void check(const maven_url *);
  };

  open_xml::open_xml(const char *path)
    : path_(path)
  {
    fd_handle fd;
    fd.open_read_only(path);
    fd_source src(fd.get());
    expat_source source(&src);
    maven_url::extract(source, urls);
  }

  std::string
  to_string(const maven_url &url)
  {
    return url.url + '|' + maven_url::to_string(url.type);
  }

  void
  open_xml::check(const maven_url *ref)
  {
    size_t i = 0;
    for (; !ref[i].url.empty(); ++i) {
      COMPARE_STRING(to_string(ref[i]), to_string(urls.at(i)));
    }
    if (i != urls.size()) {
      COMPARE_STRING(path_, "");	// fake error message
    }
    COMPARE_NUMBER(i, urls.size());
  }
}

static const maven_url geronimo_annotation[] = {
  maven_url("http://geronimo.apache.org/maven/${siteId}/${version}", maven_url::other),
  maven_url("${site.deploy.url}/maven/${siteId}/${version}", maven_url::site),
  maven_url("scm:svn:http://svn.apache.org/repos/asf/geronimo/specs/tags/geronimo-annotation_1.1_spec-1.0",
	    maven_url::connection),
  maven_url("scm:svn:https://svn.apache.org/repos/asf/geronimo/specs/tags/geronimo-annotation_1.1_spec-1.0",
	    maven_url::developerConnection),
  maven_url("http://svn.apache.org/viewcvs.cgi/geronimo/specs/tags/geronimo-annotation_1.1_spec-1.0",
	    maven_url::scm),
  maven_url()
};

static const maven_url commons_net[] = {
  maven_url("http://commons.apache.org/net/", maven_url::other),
  maven_url("http://www.apache.org/", maven_url::other),
  maven_url("http://www.apache.org/licenses/LICENSE-2.0.txt", maven_url::other),
  maven_url("scm:svn:http://svn.apache.org/repos/asf/commons/proper/net/trunk",
	    maven_url::connection),
  maven_url("scm:svn:https://svn.apache.org/repos/asf/commons/proper/net/trunk",
	    maven_url::developerConnection),
  maven_url("http://svn.apache.org/viewvc/commons/proper/net/trunk", maven_url::scm),
  maven_url("http://issues.apache.org/jira/browse/NET", maven_url::other),
  maven_url("http://vmbuild.apache.org/continuum/", maven_url::other),
  maven_url("https://repository.apache.org/service/local/staging/deploy/maven2",
	    maven_url::distributionManagement),
  maven_url("https://repository.apache.org/content/repositories/snapshots",
	    maven_url::distributionManagement),
  maven_url("scp://people.apache.org/www/commons.apache.org/net", maven_url::site),
  maven_url("http://repository.apache.org/snapshots", maven_url::repository),
  maven_url("http://repo.maven.apache.org/maven2", maven_url::repository),
  maven_url("http://repo.maven.apache.org/maven2", maven_url::pluginRepository),
  maven_url()
};

static void
test()
{
  {
    open_xml xml("test/data/maven/JPP-geronimo-annotation.pom");
    xml.check(geronimo_annotation);
  }
  {
    open_xml xml("test/data/maven/JPP-commons-net.pom");
    xml.check(commons_net);
  }
}

static test_register t("maven_url", test);

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

#include <cxxll/expat_source.hpp>
#include <cxxll/string_source.hpp>

#include "test.hpp"

using namespace cxxll;

static void
test()
{
  {
    string_source xml("<root></root>");
    expat_source src(&xml);
    CHECK(src.state() == expat_source::INIT);
    CHECK(src.next());
    CHECK(src.state() == expat_source::START);
    COMPARE_STRING(src.name(), "root");
    COMPARE_STRING(src.name_ptr(), "root");
    CHECK(src.attributes().empty());
    COMPARE_STRING(src.attribute("attribute"), "");
    CHECK(src.next());
    CHECK(src.state() == expat_source::END);
    CHECK(!src.next());
    CHECK(src.state() == expat_source::EOD);
    CHECK(!src.next());
    CHECK(src.state() == expat_source::EOD);
  }

  {
    string_source xml("<root attribute='value'>text-element</root>");
    expat_source src(&xml);
    CHECK(src.state() == expat_source::INIT);
    CHECK(src.next());
    CHECK(src.state() == expat_source::START);
    COMPARE_STRING(src.name(), "root");
    CHECK(src.attributes().size() == 1);
    COMPARE_STRING(src.attributes()["attribute"], "value");
    COMPARE_STRING(src.attribute("attribute"), "value");
    CHECK(src.next());
    CHECK(src.state() == expat_source::TEXT);
    COMPARE_STRING(src.text(), "text-element");
    CHECK(src.next());
    CHECK(src.state() == expat_source::END);
    CHECK(!src.next());
    CHECK(src.state() == expat_source::EOD);
    CHECK(!src.next());
    CHECK(src.state() == expat_source::EOD);
  }
  {
    string_source xml("<root>text-element<x/></root>");
    expat_source src(&xml);
    CHECK(src.state() == expat_source::INIT);
    CHECK(src.next());
    CHECK(src.state() == expat_source::START);
    COMPARE_STRING(src.name(), "root");
    CHECK(src.attributes().empty());
    CHECK(src.next());
    CHECK(src.state() == expat_source::TEXT);
    COMPARE_STRING(src.text(), "text-element");
    CHECK(src.next());
    CHECK(src.state() == expat_source::START);
    COMPARE_STRING(src.name(), "x");
    CHECK(src.attributes().empty());
    CHECK(src.next());
    CHECK(src.state() == expat_source::END);
    CHECK(src.next());
    CHECK(src.state() == expat_source::END);
    CHECK(!src.next());
    CHECK(src.state() == expat_source::EOD);
    CHECK(!src.next());
    CHECK(src.state() == expat_source::EOD);
  }
  {
    string_source xml("<root>text<n>-</n>element</root>");
    expat_source src(&xml);
    CHECK(src.state() == expat_source::INIT);
    CHECK(src.next());
    CHECK(src.state() == expat_source::START);
    COMPARE_STRING(src.name(), "root");
    CHECK(src.attributes().empty());
    CHECK(src.next());
    CHECK(src.state() == expat_source::TEXT);
    COMPARE_STRING(src.text(), "text");
    CHECK(src.next());
    CHECK(src.state() == expat_source::START);
    COMPARE_STRING(src.name(), "n");
    CHECK(src.attributes().empty());
    CHECK(src.next());
    CHECK(src.state() == expat_source::TEXT);
    COMPARE_STRING(src.text(), "-");
    CHECK(src.next());
    CHECK(src.state() == expat_source::END);
    CHECK(src.next());
    CHECK(src.state() == expat_source::TEXT);
    COMPARE_STRING(src.text(), "element");
    CHECK(src.next());
    CHECK(src.state() == expat_source::END);
    CHECK(~src.next());
    CHECK(src.state() == expat_source::EOD);
    CHECK(!src.next());
    CHECK(src.state() == expat_source::EOD);
  }
  {
    string_source xml("<root a1=\"v1\" a2=\"v2\"></root>");
    expat_source src(&xml);
    CHECK(src.state() == expat_source::INIT);
    CHECK(src.next());
    CHECK(src.state() == expat_source::START);
    COMPARE_STRING(src.name(), "root");
    CHECK(src.attributes().size() == 2);
    COMPARE_STRING(src.attributes()["a1"], "v1");
    COMPARE_STRING(src.attributes()["a2"], "v2");
    COMPARE_STRING(src.attributes()["a3"], "");
    COMPARE_STRING(src.attribute("a1"), "v1");
    COMPARE_STRING(src.attribute("a2"), "v2");
    COMPARE_STRING(src.attribute("a3"), "");
    std::map<std::string, std::string> m;
    src.attributes(m);
    CHECK(m == src.attributes());
    CHECK(src.next());
    CHECK(src.state() == expat_source::END);
    CHECK(!src.next());
    CHECK(src.state() == expat_source::EOD);
    CHECK(!src.next());
    CHECK(src.state() == expat_source::EOD);
  }
  {
    string_source xml("<root>ab<!-- cd -->ef<n/></root>");
    expat_source src(&xml);
    CHECK(src.next());
    COMPARE_STRING(src.name(), "root");
    CHECK(src.next());
    COMPARE_STRING(src.text_and_next(), "abef");
    COMPARE_STRING(src.name(), "n");
    CHECK(src.next());
    CHECK(src.state() == expat_source::END);
    CHECK(src.next());
    CHECK(src.state() == expat_source::END);
    CHECK(!src.next());
    CHECK(src.state() == expat_source::EOD);
  }
  {
    string_source xml("<root>ab<!-- cd -->ef<n/>?</root>");
    expat_source src(&xml);
    CHECK(src.next());
    COMPARE_STRING(src.name(), "root");
    CHECK(src.next());
    src.skip(); // "abef"
    CHECK(src.state() == expat_source::START);
    COMPARE_STRING(src.name(), "n");
    src.skip(); // <n/>
    CHECK(src.state() == expat_source::TEXT);
    COMPARE_STRING(src.text_and_next(), "?");
    CHECK(src.state() == expat_source::END);
  }
  {
    string_source xml("<root>ab<n>x<m>y</m>z</n>?</root>");
    expat_source src(&xml);
    CHECK(src.next());
    COMPARE_STRING(src.name(), "root");
    CHECK(src.next());
    src.skip(); // "ab"
    CHECK(src.state() == expat_source::START);
    COMPARE_STRING(src.name(), "n");
    CHECK(src.next());
    CHECK(src.state() == expat_source::TEXT);
    COMPARE_STRING(src.text(), "x");
    src.unnest();
    COMPARE_STRING(src.text(), "?");
    CHECK(src.state() == expat_source::TEXT);
    src.unnest();
    CHECK(!src.next());
    CHECK(src.state() == expat_source::EOD);
    src.unnest();
    CHECK(src.state() == expat_source::EOD);
  }
}

static test_register t("expat_source", test);

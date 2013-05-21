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

#include <cxxll/string_source.hpp>
#include <cxxll/expat_source.hpp>
#include <cxxll/expat_minidom.hpp>

#include "test.hpp"

using namespace cxxll;
using namespace expat_minidom;

static void
check_helper(element &e)
{
  COMPARE_STRING(e.name, "root");
  CHECK(e.attributes.empty());
  CHECK(e.children.size() == 3);
  COMPARE_STRING(e.text(), "abef");
  COMPARE_STRING(std::tr1::dynamic_pointer_cast<text>(e.children.at(0))->data,
		 "ab");
  COMPARE_STRING(std::tr1::dynamic_pointer_cast<element>(e.children.at(1))->name,
		 "n");
  COMPARE_STRING(std::tr1::dynamic_pointer_cast<text>(e.children.at(2))->data,
		 "ef");
}

static void
test()
{

  {
    string_source xml("<root/>");
    expat_source src(&xml);
    std::tr1::shared_ptr<element> e(parse(src));
    COMPARE_STRING(e->name, "root");
    CHECK(e->attributes.empty());
    CHECK(e->children.empty());
    CHECK(src.state() == expat_source::EOD);
  }
  {
    string_source xml("<root>ab<!-- cd -->ef</root>");
    expat_source src(&xml);
    std::tr1::shared_ptr<element> e(parse(src));
    COMPARE_STRING(e->name, "root");
    CHECK(e->attributes.empty());
    CHECK(e->children.size() == 1);
    COMPARE_STRING(e->text(), "abef");
    COMPARE_STRING(std::tr1::dynamic_pointer_cast<text>(e->children.front())->data,
		   "abef");
    CHECK(src.state() == expat_source::EOD);
  }
  {
    string_source xml("<root>ab<n/>ef</root>");
    expat_source src(&xml);
    std::tr1::shared_ptr<element> e(parse(src));
    check_helper(*e);
    CHECK(src.state() == expat_source::EOD);
  }
  {
    string_source xml("<outer>!<root>ab<n/>ef</root>?</outer>");
    expat_source src(&xml);
    CHECK(src.next());
    COMPARE_STRING(src.name(), "outer");
    CHECK(src.next());
    COMPARE_STRING(src.text(), "!");
    CHECK(src.next());
    COMPARE_STRING(src.name(), "root");
    std::tr1::shared_ptr<element> e(parse(src));
    check_helper(*e);
    COMPARE_STRING(src.text(), "?");
    CHECK(src.next());
    CHECK(src.state() == expat_source::END);
    CHECK(!src.next());
    CHECK(src.state() == expat_source::EOD);
  }
}

static test_register t("expat_minidom", test);

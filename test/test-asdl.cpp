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

#include <cxxll/asdl.hpp>
#include "test.hpp"

#include <sstream>

using namespace cxxll;

static void
test_identifier()
{
  asdl::identifier id;
  CHECK(!asdl::valid_identifier(id));
  CHECK(!asdl::valid_type(id));
  CHECK(!asdl::valid_constructor(id));
  id = "1";
  CHECK(!asdl::valid_identifier(id));
  CHECK(!asdl::valid_type(id));
  CHECK(!asdl::valid_constructor(id));
  id = "1_";
  CHECK(!asdl::valid_identifier(id));
  CHECK(!asdl::valid_type(id));
  CHECK(!asdl::valid_constructor(id));
  id = "_a";
  CHECK(!asdl::valid_identifier(id));
  CHECK(!asdl::valid_type(id));
  CHECK(!asdl::valid_constructor(id));
  id = "typ";
  CHECK(asdl::valid_identifier(id));
  CHECK(asdl::valid_type(id));
  CHECK(!asdl::valid_constructor(id));
  id = "CON";
  CHECK(asdl::valid_identifier(id));
  CHECK(!asdl::valid_type(id));
  CHECK(asdl::valid_constructor(id));
  id = "typ1";
  CHECK(asdl::valid_identifier(id));
  CHECK(asdl::valid_type(id));
  CHECK(!asdl::valid_constructor(id));
  id = "CON1";
  CHECK(asdl::valid_identifier(id));
  CHECK(!asdl::valid_type(id));
  CHECK(asdl::valid_constructor(id));
  id = "t";
  CHECK(asdl::valid_identifier(id));
  CHECK(asdl::valid_type(id));
  CHECK(!asdl::valid_constructor(id));
  id = "C";
  CHECK(asdl::valid_identifier(id));
  CHECK(!asdl::valid_type(id));
  CHECK(asdl::valid_constructor(id));
  id = "t_";
  CHECK(asdl::valid_identifier(id));
  CHECK(asdl::valid_type(id));
  CHECK(!asdl::valid_constructor(id));
  id = "C_";
  CHECK(asdl::valid_identifier(id));
  CHECK(!asdl::valid_type(id));
  CHECK(asdl::valid_constructor(id));
}

static void
test_field()
{
  std::ostringstream os;
  asdl::field fld;
  CHECK(fld.cardinality == asdl::single);
  fld.type = "int";
  os << fld;
  COMPARE_STRING(os.str(), "int");
  fld.name = "field";
  os.str(std::string());
  os << fld;
  COMPARE_STRING(os.str(), "int field");
  fld.cardinality = asdl::optional;
  os.str(std::string());
  os << fld;
  COMPARE_STRING(os.str(), "int? field");
  fld.cardinality = asdl::multiple;
  os.str(std::string());
  os << fld;
  COMPARE_STRING(os.str(), "int* field");
}

static void
test_constructor()
{
  std::ostringstream os;
  asdl::field fld;
  asdl::constructor con;

  con.name = "CON";
  os << con;
  COMPARE_STRING(os.str(), "CON");
  fld.type = "int";
  con.fields.push_back(fld);
  os.str(std::string());
  os << con;
  COMPARE_STRING(os.str(), "CON(int)");
  fld.type = "string";
  fld.cardinality = asdl::optional;
  con.fields.push_back(fld);
  os.str(std::string());
  os << con;
  COMPARE_STRING(os.str(), "CON(int,string?)");
}

static void
test_product()
{
  asdl::definition product;
  asdl::field fld;
  std::ostringstream os;

  product.name = "product";
  os << product;
  COMPARE_STRING(os.str(), "product = ()");
  os.str(std::string());
  os << product.declarator;
  COMPARE_STRING(os.str(), "()");

  fld.type = "int";
  product.declarator.fields.push_back(fld);
  os.str(std::string());
  os << product;
  COMPARE_STRING(os.str(), "product = (int)");
  os.str(std::string());
  os << product.declarator;
  COMPARE_STRING(os.str(), "(int)");

  fld.type = "string";
  fld.cardinality = asdl::multiple;
  product.declarator.fields.push_back(fld);
  os.str(std::string());
  os << product;
  COMPARE_STRING(os.str(), "product = (int,string*)");
  os.str(std::string());
  os << product.declarator;
  COMPARE_STRING(os.str(), "(int,string*)");

  fld.type = "double";
  fld.name = "value";
  fld.cardinality = asdl::single;
  product.declarator.fields.push_back(fld);
  os.str(std::string());
  os << product;
  COMPARE_STRING(os.str(), "product = (int,string*,double value)");
  os.str(std::string());
  os << product.declarator;
  COMPARE_STRING(os.str(), "(int,string*,double value)");
}

static void
test_sum()
{
  asdl::definition sum;
  asdl::field fld;
  asdl::constructor con;
  std::ostringstream os;

  sum.name = "sum";
  con.name = "CON";
  sum.declarator.constructors.push_back(con);
  os << sum;
  COMPARE_STRING(os.str(), "sum = CON");
  os.str(std::string());
  os << sum.declarator;
  COMPARE_STRING(os.str(), "CON");

  con.name = "DECON";
  fld.type = "int";
  con.fields.push_back(fld);
  sum.declarator.constructors.push_back(con);
  os.str(std::string());
  os << sum;
  COMPARE_STRING(os.str(), "sum = CON | DECON(int)");
  os.str(std::string());
  os << sum.declarator;
  COMPARE_STRING(os.str(), "CON | DECON(int)");

  fld.type = "string";
  sum.declarator.fields.push_back(fld);
  os.str(std::string());
  os << sum;
  COMPARE_STRING(os.str(), "sum = CON | DECON(int) attributes (string)");
  os.str(std::string());
  os << sum.declarator;
  COMPARE_STRING(os.str(), "CON | DECON(int) attributes (string)");
}

static void
test()
{
  test_identifier();
  test_field();
  test_constructor();
  test_product();
  test_sum();
}

static test_register t("asdl", test);

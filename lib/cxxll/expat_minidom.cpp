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

#include <cxxll/expat_minidom.hpp>
#include <cxxll/expat_source.hpp>

using namespace cxxll;

expat_minidom::node::~node()
{
}

expat_minidom::text::text()
{
}

expat_minidom::text::~text()
{
}

expat_minidom::element::element()
{
}

expat_minidom::element::~element()
{
}

expat_minidom::element *
expat_minidom::element::first_child(const char *name)
{
  for(std::vector<std::tr1::shared_ptr<expat_minidom::node> >::iterator
	p = children.begin(), end = children.end(); p != end; ++p) {
    element *e = dynamic_cast<element *>(p->get());
    if (e && e->name == name) {
      return e;
    }
  }
  return 0;
}

std::string
expat_minidom::element::text() const
{
  std::string r;
  for(std::vector<std::tr1::shared_ptr<expat_minidom::node> >::const_iterator
	p = children.begin(), end = children.end(); p != end; ++p) {
    expat_minidom::text *t = dynamic_cast<expat_minidom::text *>(p->get());
    if (t) {
      r += t->data;
    }
  }
  return r;
}

std::tr1::shared_ptr<expat_minidom::element>
expat_minidom::parse(expat_source &source)
{
  if (source.state() == expat_source::INIT) {
    source.next();
  }
  source.name(); // ensure that we are in state START

  std::vector<std::tr1::shared_ptr<element> > stack;
  while (true) {
    if (source.state() == expat_source::START) {
      std::tr1::shared_ptr<element> n(new element);
      n->name = source.name_ptr();
      source.attributes(n->attributes);
      if (!stack.empty()) {
	stack.back()->children.push_back(n);
      }
      stack.push_back(n);
      source.next();
    } else if (source.state() == expat_source::END) {
      source.next();
      if (stack.size() == 1) {
	return stack.back();
      } else {
	stack.pop_back();
      }
    } else {
      std::tr1::shared_ptr<text> n(new text);
      stack.back()->children.push_back(n);
      while (source.state() == expat_source::TEXT) {
	n->data += source.text_ptr();
	source.next();
      }
    }
  }
}

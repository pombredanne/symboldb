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

#include "expat_minidom.hpp"
#include "expat_handle.hpp"

expat_minidom::node::~node()
{
}

expat_minidom::text::text(const char *buffer, size_t length)
  : data(buffer, length)
{
}

expat_minidom::text::~text()
{
}

expat_minidom::element::element(const char *nam, const char **attrs)
{
  name = nam;
  for (; *attrs; attrs += 2) {
    attributes[attrs[0]] = attrs[1];
  }
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

namespace {
  using namespace expat_minidom;

  struct context {
    std::string &error;
    expat_handle xml;
    std::tr1::shared_ptr<element> root;
    std::vector<element *> open_elements;
    std::string *open_text;
    bool bad_alloc;

    context(std::string &err)
      : error(err), open_text(0), bad_alloc(false)
    {
      // EntityDeclHandler needs a reference to the parser to stop
      // parsing.
      XML_SetUserData(xml.raw, this);
      // Disable entity processing, to inhibit entity expansion.
      XML_SetEntityDeclHandler(xml.raw, EntityDeclHandler);

      XML_SetElementHandler(xml.raw, StartElementHandler, EndElementHandler);
      XML_SetCharacterDataHandler(xml.raw, CharacterDataHandler);
    }

    // Stop the parser when an entity declaration is encountered.
    static void
    EntityDeclHandler(void *userData,
		      const XML_Char *, int,
		      const XML_Char *, int,
		      const XML_Char *, const XML_Char *,
		      const XML_Char *, const XML_Char *)
    {
      context *ctx = static_cast<context *>(userData);
      try {
	ctx->error = "entity declaration not allowed";
      } catch (std::bad_alloc &) {
	ctx->bad_alloc = true;
      }
      XML_StopParser(ctx->xml.raw, XML_FALSE);
    }

    static void
    StartElementHandler(void *userData,
			const XML_Char *name, const XML_Char **attrs)
    {
      context *ctx = static_cast<context *>(userData);
      try {
	std::tr1::shared_ptr<element> p(new element(name, attrs));
	if (ctx->root) {
	  ctx->open_elements.back()->children.push_back(p);
	  ctx->open_text = 0;
	} else {
	  ctx->root = p;
	}
	ctx->open_elements.push_back(p.get());
      } catch (std::bad_alloc &) {
	ctx->bad_alloc = true;
	XML_StopParser(ctx->xml.raw, XML_FALSE);
      }
    }

    static void
    EndElementHandler(void *userData, const XML_Char *)
    {
      context *ctx = static_cast<context *>(userData);
      ctx->open_elements.pop_back();
      ctx->open_text = 0;
    }

    static void
    CharacterDataHandler(void *userData, const XML_Char *s, int len)
    {
      context *ctx = static_cast<context *>(userData);
      try {
	if (ctx->open_text != 0) {
	  ctx->open_text->append(s, len);
	} else {
	  std::tr1::shared_ptr<text> p(new text(s, len));
	  ctx->open_elements.back()->children.push_back(p);
	  ctx->open_text = &p->data;
	}
      } catch (std::bad_alloc &) {
	ctx->bad_alloc = true;
	XML_StopParser(ctx->xml.raw, XML_FALSE);
      }
    }

    void
    run(const char *buffer, size_t length)
    {
      enum XML_Status status = XML_Parse(xml.raw, buffer, length,
					 /* isFinal */ 1);
      if (bad_alloc) {
	throw std::bad_alloc();
      }
      if (error.empty() && status != XML_STATUS_OK) {
	error = XML_ErrorString(XML_GetErrorCode(xml.raw));
      }
    }
  };
}

std::tr1::shared_ptr<expat_minidom::element>
expat_minidom::parse(const char *buffer, size_t length,
		     std::string &error)
{
  context ctx(error);
  ctx.run(buffer, length);
  if (ctx.error.empty()) {
    return ctx.root;
  }
  return std::tr1::shared_ptr<element>();
}

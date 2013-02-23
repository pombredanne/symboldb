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

#include "repomd.hpp"
#include "expat_source.hpp"

struct repomd::primary::impl {
  expat_source source_;
  std::string name_;
  std::string sourcerpm_;

  impl(source *src)
    : source_(src)
  {
    source_.next();
    if (source_.name() != "metadata") {
      // FIXME: proper exception
      throw std::runtime_error("invalid root element: " + source_.name());
    }
    source_.next(); // in <metadata>
  }

  void clear()
  {
    name_.clear();
    sourcerpm_.clear();
  }

  void validate()
  {
    // FIXME: proper exception
    if (name_.empty()) {
      throw std::runtime_error("missing <name> element");
    }
    if (sourcerpm_.empty()) {
      throw std::runtime_error
	("missing <format>/<rpm:sourcerpm> element for " + name_);
    }
  }

  bool next()
  {
    clear();

    while (true) {
      if (source_.state() == expat_source::END) {
	return false;
      } else if (source_.state() == expat_source::START
		 && source_.name() == "package") {
	break;
      } else {
	source_.skip();
      }
    }

    // At <package>.
    if (source_.attribute("type") != "rpm") {
      // FIXME: proper exception
      throw std::runtime_error("invalid package type: "
			       + source_.attribute("type"));
    }
    source_.next();

    // Inside <package>.
    while (source_.state() != expat_source::END) {
      if (source_.state() == expat_source::TEXT) {
	// Ignore whitespace.
	source_.skip();
	continue;
      }
      std::string tag(source_.name());
      if (tag == "name") {
	source_.next();
	name_ = source_.text_and_next();
	source_.unnest();
      } else if (tag == "format") {
	process_format();
      } else {
	source_.skip();
      }
    }
    source_.next(); // leave <package>

    validate();
    return true;
  }

  void process_format()
  {
    source_.next();
    // Inside <format>.
    while (source_.state() != expat_source::END) {
      if (source_.state() == expat_source::TEXT) {
	// Ignore whitespace.
	source_.skip();
	continue;
      }
      if (source_.name() == "rpm:sourcerpm") {
	source_.next();
	sourcerpm_ = source_.text_and_next();
	source_.unnest();
      } else {
	source_.skip();
      }
    }
    source_.next(); // leaving <format>
  }
};

repomd::primary::primary(source *src)
  : impl_(new impl(src))
{
}

repomd::primary::~primary()
{
}

bool
repomd::primary::next()
{
  return impl_->next();
}

const std::string &
repomd::primary::name() const
{
  return impl_->name_;
}

const std::string &
repomd::primary::sourcerpm() const
{
  return impl_->sourcerpm_;
}

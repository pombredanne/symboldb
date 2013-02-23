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
#include "rpm_package_info.hpp"
#include "string_support.hpp"
#include "checksum.hpp"

struct repomd::primary::impl {
  expat_source source_;
  rpm_package_info info_;
  std::string href_;
  ::checksum checksum_;

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
    info_.name.clear();
    info_.version.clear();
    info_.release.clear();
    info_.arch.clear();
    info_.source_rpm.clear();
    info_.hash.clear();
    info_.epoch = -1;
    href_.clear();
    checksum_.type.clear();
    checksum_.value.clear();
    checksum_.length = ::checksum::no_length;
  }

  void validate()
  {
    // FIXME: proper exception
    if (info_.name.empty()) {
      throw std::runtime_error("missing <name> element");
    }
    check_attr("<version>", info_.version); // covers release
    check_attr("<arch>", info_.arch);
    check_attr("<format>/<rpm:sourcerpm>", info_.source_rpm);
    check_attr("<location>/href", href_);
    check_attr("<checksum>", checksum_.type);
    if (checksum_.length == ::checksum::no_length) {
      throw std::runtime_error("missing <size> element");
    }
  }

  void check_attr(const char *name, const std::string &);

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
	info_.name = source_.text_and_next();
	source_.unnest();
      } else if (tag == "arch") {
	source_.next();
	info_.arch = source_.text_and_next();
	source_.unnest();
      } else if (tag == "version") {
	process_version();
      } else if (tag == "checksum") {
	std::string type(source_.attribute("type"));
	source_.next();
	// FIXME: proper exception
	checksum_.set_hexadecimal(type.c_str(), checksum_.length,
				  source_.text_and_next().c_str());
	source_.unnest();
      } else if (tag == "size") {
	// FIXME: proper exception
	parse_unsigned_long_long(source_.attribute("package"),
				 checksum_.length);
	source_.skip();
      } else if (tag == "location") {
	href_ = source_.attribute("href");
	source_.skip();
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

  void process_version()
  {
    info_.version = strip(source_.attribute("ver"));
    info_.release = strip(source_.attribute("rel"));
    unsigned long long epoch;
    std::string epochstr(strip(source_.attribute("epoch")));
    if (!parse_unsigned_long_long(epochstr, epoch)
	|| epoch != (static_cast<unsigned long long>
		     (static_cast<int>(epoch)))) {
      // FIXME: proper exception
      info_.version.clear();
    } else {
      info_.epoch = epoch;
    }
    source_.skip();
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
	info_.source_rpm = source_.text_and_next();
	source_.unnest();
      } else {
	source_.skip();
      }
    }
    source_.next(); // leaving <format>
  }
};

void
repomd::primary::impl::check_attr(const char *name, const std::string &value)
{
  if (value.empty()) {
    std::string msg("missing ");
    msg += name;
    msg += " element in package: ";
    msg += info_.name;
    throw std::runtime_error(msg); // FIXME
  }
}

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

const rpm_package_info &
repomd::primary::info() const
{
  return impl_->info_;
}

const checksum &
repomd::primary::checksum() const
{
  return impl_->checksum_;
}

const std::string &
repomd::primary::href() const
{
  return impl_->href_;
}

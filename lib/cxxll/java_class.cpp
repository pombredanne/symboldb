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

#include <cxxll/java_class.hpp>
#include <cxxll/vector_extract.hpp>

#include <cstdio>

bool
cxxll::java_class::has_signature(const std::vector<unsigned char> &vec)
{
  if (vec.size() < 32) {
    return false;
  }
  size_t offset = 0;
  unsigned magic;
  unsigned short minor, major;
  using namespace cxxll::big_endian;
  extract(vec, offset, magic);
  extract(vec, offset, minor);
  extract(vec, offset, major);
  return magic == 0xCAFEBABEU && major < 100;
}

cxxll::java_class::java_class(const std::vector<unsigned char> *vec)
  : buffer_(vec)
{
  size_t offset = 0;
  try {
    using namespace cxxll::big_endian;
    {
      unsigned magic;
      extract(*vec, offset, magic);
      if (magic != 0xCAFEBABEU) {
	throw exception("class file magic value not found");
      }
    }
    extract(*vec, offset, minor_version_);
    extract(*vec, offset, major_version_);

    {
      unsigned short constant_pool_count;
      extract(*vec, offset, constant_pool_count);
      constant_pool_offsets.reserve(constant_pool_count - 1);
      for (unsigned i = 1; i < constant_pool_count; ++i ) {
	constant_pool_offsets.push_back(offset);
	unsigned char tag;
	extract(*vec, offset, tag);
	switch (tag) {
	case CONSTANT_Class:
	case CONSTANT_String:
	  offset += 2;
	  break;
	case CONSTANT_Fieldref:
	case CONSTANT_Float:
	case CONSTANT_Integer:
	case CONSTANT_InterfaceMethodref:
	case CONSTANT_Methodref:
	case CONSTANT_NameAndType:
	  offset += 4;
	  break;
	case CONSTANT_Long:
	case CONSTANT_Double:
	  offset += 8;
	  // longs and doubles take two slots.
	  constant_pool_offsets.push_back(0);
	  ++i;
	  break;
	case CONSTANT_Utf8:
	  {
	    unsigned short len;
	    extract(*vec, offset, len);
	    offset += len;
	  }
	  break;
	default:
	  {
	    char buf[64];
	    snprintf(buf, sizeof(buf), "invalid constant pool tag %u",
		     (unsigned) tag);
	    throw exception(buf);
	  }
	}
      }
    }

    extract(*vec, offset, access_flags_);
    extract(*vec, offset, this_class_);
    extract(*vec, offset, super_class_);

    {
      unsigned short interface_count;
      extract(*vec, offset, interface_count);
      interface_indexes.resize(interface_count);
      for (unsigned i = 0; i < interface_count; ++i) {
	unsigned short idx;
	extract(*vec, offset, idx);
	interface_indexes.at(i) = idx;
      }
    }

    // TODO: fields, methods, attributes

  } catch (std::out_of_range &) {
    char buf[64];
    snprintf(buf, sizeof(buf), "index out of range at %zu", offset);
    throw exception(buf);
  }
}

cxxll::java_class::~java_class()
{
}

std::string
cxxll::java_class::this_class() const
{
  return class_name(this_class_);
}

std::string
cxxll::java_class::super_class() const
{
  if (super_class_ == 0) {
    return std::string();
  }
  return class_name(super_class_);
}

std::string
cxxll::java_class::class_name(unsigned short idx) const
{
  if (idx == 0) {
    throw exception("zero constant pool index");
  }
  size_t offset = constant_pool_offsets.at(idx - 1);
  unsigned char tag;
  extract(buffer(), offset, tag);
  if (tag != CONSTANT_Class) {
    throw exception("class tag expected");
  }
  unsigned short name_idx;
  big_endian::extract(buffer(), offset, name_idx);
  return utf8_string(name_idx);
}

std::string
cxxll::java_class::utf8_string(unsigned short idx) const
{
  if (idx == 0) {
    throw exception("zero constant pool index");
  }
  size_t offset = constant_pool_offsets.at(idx - 1);
  unsigned char tag;
  extract(buffer(), offset, tag);
  if (tag != CONSTANT_Utf8) {
    throw exception("UTF-8 tag expected");
  }
  unsigned short len;
  big_endian::extract(buffer(), offset, len);
  std::string result;
  extract(buffer(), offset, len, result);
  return result;
}

unsigned
cxxll::java_class::interface_count() const
{
  return interface_indexes.size();
}

std::string
cxxll::java_class::interface(unsigned index) const
{
  return class_name(interface_indexes.at(index));
}

std::vector<std::string>
cxxll::java_class::class_references() const
{
  std::vector<std::string> result;
  for (std::vector<unsigned>::const_iterator
	 p = constant_pool_offsets.begin(), end = constant_pool_offsets.end();
       p != end; ++p) {
    if (*p != 0 && buffer().at(*p) == CONSTANT_Class) {
      size_t offset = *p + 1;
      unsigned short name_idx;
      big_endian::extract(buffer(), offset, name_idx);
      result.push_back(utf8_string(name_idx));
    }
  }
  return result;
}

//////////////////////////////////////////////////////////////////////
// cxxll::java_class::exception

cxxll::java_class::exception::exception(const char *what)
  : what_(what)
{
}

cxxll::java_class::exception::~exception() throw()
{
}

const char *
cxxll::java_class::exception::what() const throw()
{
  return what_.c_str();
}

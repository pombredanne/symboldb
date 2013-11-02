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
#include <cxxll/expat_handle.hpp>
#include <cxxll/source.hpp>
#include <cxxll/string_support.hpp>
#include <cxxll/raise.hpp>
#include <cxxll/const_stringref.hpp>

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>
#include <sstream>

using namespace cxxll;

// We translate the callback-based Expat API into an event-based API.
// We buffer callback events in a vector, using the following
// encoding.  The first byte of every element encodes the type.  We
// use the fact that Expat cannot handle embedded NULs.  This encoding
// is entirely internal.

enum {
  // Start of a start element.  The element name follows as a
  // NUL-terminated string.  Then zero or more ENC_ATTRIBUTE elements,
  // then nothing or a non-ENC_ATTRIBUTE elemement.
  ENC_START = 1,

  // An attribute pair.  Followed by two NUL-terminated strings, key
  // and value.
  ENC_ATTRIBUTE,

  // End of an element.  Nothing follows.
  ENC_END,

  // Followed by the text content as a NUL-terminated string.
  ENC_TEXT,

  // End of stream.
  ENC_EOD
};

struct expat_source::impl {
  expat_handle handle_;
  source *source_;
  size_t consumed_bytes_{};
  std::vector<char> upcoming_;
  size_t upcoming_pos_{};

  struct position {
    unsigned line{};
    unsigned column{};
  };
  position position_;

  size_t elem_start_;
  size_t elem_len_;		// only covers the name for START
  size_t attr_start_;
  std::string error_;
  state_type state_{INIT};
  bool bad_alloc_{};
  bool bad_entity_{};

  impl(source *);

  // Is there more data in upcoming_?
  bool remaining() const;

  // Returns the character at upcoming_pos_.  remaining() must be
  // true.
  char tag() const;

  // Returns a strign pointer to the character at upcoming_pos_.
  const char *string_at_pos() const;

  // Returns a pointer one past the end of upcoming_.
  const char *upcoming_end() const;

  // Appends the NUL-terminated string to upcoming_.
  void append_cstr(const char *);

  // Throws illegal_state if the state type does not match state_.
  void check_state(state_type) const;

  // Removes the data before upcoming_pos_ from the upcoming_ vector,
  // and sets upcoming_pos_ to zero.
  void compact();

  // Retrieves more upcoming data via Expat.
  void feed();

  // Throws an exception if an error occured.
  void check_error(enum XML_Status status, const_stringref buf);

  // Adds line and column to the end of upcoming_, based on the state
  // of handle_.
  void push_position();

  // Reads position_ from upcoming_ (at upcoming_pos_).
  void pop_position();

  // Expat callback functions.
  static void EntityDeclHandler(void *userData,
				const XML_Char *, int,
				const XML_Char *, int,
				const XML_Char *, const XML_Char *,
				const XML_Char *, const XML_Char *);
  static void StartElementHandler(void *userData,
				  const XML_Char *name,
				  const XML_Char **attrs);
  static void EndElementHandler(void *userData, const XML_Char *);
  static void CharacterDataHandler(void *userData,
				   const XML_Char *s, int len);
};

inline
expat_source::impl::impl(source *src)
  : source_(src)
{
  XML_SetUserData(handle_.raw, this);
  XML_SetEntityDeclHandler(handle_.raw, EntityDeclHandler);
  XML_SetElementHandler(handle_.raw, StartElementHandler, EndElementHandler);
  XML_SetCharacterDataHandler(handle_.raw, CharacterDataHandler);
}

inline bool
expat_source::impl::remaining() const
{
  return upcoming_pos_ < upcoming_.size();
}

inline char
expat_source::impl::tag() const
{
  return upcoming_.at(upcoming_pos_);
}

inline const char *
expat_source::impl::string_at_pos() const
{
  return upcoming_.data() + upcoming_pos_;
}

inline const char *
expat_source::impl::upcoming_end() const
{
  return upcoming_.data() + upcoming_.size();
}

void
expat_source::impl::append_cstr(const char *str)
{
  upcoming_.insert(upcoming_.end(), str, str + strlen(str) + 1);
}

void
expat_source::impl::check_state(state_type expected) const
{
  if (state_ != expected) {
    throw illegal_state(this, expected);
  }
}

void
expat_source::impl::feed()
{
  assert(upcoming_pos_ == upcoming_.size());
  upcoming_.clear();
  upcoming_pos_ = 0;
  char buf[4096];
  do {
    size_t ret = source_->read(reinterpret_cast<unsigned char *>(buf),
			       sizeof(buf));
    check_error(XML_Parse(handle_.raw, buf, ret, /* isFinal */ ret == 0),
		const_stringref(buf, ret));
    consumed_bytes_ += ret;
    if (ret == 0) {
      upcoming_.push_back(ENC_EOD);
    }
  } while(upcoming_.empty());
}

void
expat_source::impl::check_error(enum XML_Status status, const_stringref buf)
{
  if (bad_alloc_) {
    raise<std::bad_alloc>();
  }
  if (bad_entity_) {
    throw entity_declaration(this, buf);
  }
  if (!error_.empty()) {
    throw source_error(this);
  }
  if (status != XML_STATUS_OK) {
    throw malformed(this, buf);
  }
}

void
expat_source::impl::push_position()
{
  position pos;
  pos.line = XML_GetCurrentLineNumber(handle_.raw);
  pos.column = XML_GetCurrentColumnNumber(handle_.raw);
  unsigned char *buf = reinterpret_cast<unsigned char *>(&pos);
  upcoming_.insert(upcoming_.end(), buf, buf + sizeof(pos));
}

void
expat_source::impl::pop_position()
{
  if (upcoming_.size() - upcoming_pos_ < sizeof(position_)) {
    abort();
  }
  std::copy(upcoming_.begin() + upcoming_pos_,
	    upcoming_.begin() + upcoming_pos_ + sizeof(position_),
	    reinterpret_cast<unsigned char *>(&position_));
  upcoming_pos_ += sizeof(position_);
}

//////////////////////////////////////////////////////////////////////
// Expat callbacks

void
expat_source::impl::EntityDeclHandler(void *userData,
				      const XML_Char *, int,
				      const XML_Char *, int,
				      const XML_Char *, const XML_Char *,
				      const XML_Char *, const XML_Char *)
{
  // Stop the parser when an entity declaration is encountered.
  impl *impl_ = static_cast<impl *>(userData);
  impl_->bad_entity_ = true;
  XML_StopParser(impl_->handle_.raw, XML_FALSE);
}

void
expat_source::impl::StartElementHandler(void *userData,
					const XML_Char *name,
					const XML_Char **attrs)
{
  impl *impl_ = static_cast<impl *>(userData);
  try {
    impl_->upcoming_.push_back(ENC_START);
    impl_->push_position();
    impl_->append_cstr(name);
    while (*attrs) {
      impl_->upcoming_.push_back(ENC_ATTRIBUTE);
      impl_->append_cstr(*attrs);
      ++attrs;
      impl_->append_cstr(*attrs);
      ++attrs;
    }
  } catch (std::bad_alloc &) {
    impl_->bad_alloc_ = true;
    XML_StopParser(impl_->handle_.raw, XML_FALSE);
  }
}

void
expat_source::impl::EndElementHandler(void *userData, const XML_Char *)
{
  impl *impl_ = static_cast<impl *>(userData);
  try {
    impl_->upcoming_.push_back(ENC_END);
    impl_->push_position();
  } catch (std::bad_alloc &) {
    impl_->bad_alloc_ = true;
    XML_StopParser(impl_->handle_.raw, XML_FALSE);
  }
}

void
expat_source::impl::CharacterDataHandler(void *userData,
					 const XML_Char *s, int len)
{
  // Expat should error out on embedded NUL characters.
  assert(memchr(s, 0, len) == nullptr);
  impl *impl_ = static_cast<impl *>(userData);
  try {
    impl_->upcoming_.push_back(ENC_TEXT);
    impl_->push_position();
    impl_->upcoming_.insert(impl_->upcoming_.end(), s, s + len);
    impl_->upcoming_.push_back(0);
  } catch (std::bad_alloc &) {
    impl_->bad_alloc_ = true;
    XML_StopParser(impl_->handle_.raw, XML_FALSE);
  }
}

//////////////////////////////////////////////////////////////////////
// expat_source

expat_source::expat_source(source *src)
  : impl_(new impl(src))
{
}

expat_source::~expat_source()
{
}

bool
expat_source::next()
{
  if (impl_->state_ == EOD) {
    return false;
  }
  if (!impl_->remaining()) {
    impl_->feed();
  }

  // The following decoder assumes that the source is well-formed and
  // trusted.
  switch (impl_->tag()) {
  case ENC_START:
    ++impl_->upcoming_pos_;
    impl_->pop_position();
    impl_->elem_start_ = impl_->upcoming_pos_;
    impl_->elem_len_ = strlen(impl_->string_at_pos());
    impl_->upcoming_pos_ += impl_->elem_len_ + 1;
    impl_->attr_start_ = impl_->upcoming_pos_;
    while (impl_->remaining() && impl_->tag() == ENC_ATTRIBUTE) {
      // Tag is skipped implicitly along with the key, and the value follows.
      impl_->upcoming_pos_ += strlen(impl_->string_at_pos()) + 1;
      impl_->upcoming_pos_ += strlen(impl_->string_at_pos()) + 1;
    }
    impl_->state_ = START;
    break;
  case ENC_TEXT:
    ++impl_->upcoming_pos_;
    impl_->pop_position();
    impl_->elem_start_ = impl_->upcoming_pos_;
    impl_->elem_len_ = strlen(impl_->string_at_pos());
    impl_->upcoming_pos_ += impl_->elem_len_ + 1;
    impl_->state_ = TEXT;
    break;
  case ENC_END:
    ++impl_->upcoming_pos_;
    impl_->pop_position();
    impl_->state_ = END;
    break;
  case ENC_EOD:
    ++impl_->upcoming_pos_;
    impl_->state_ = EOD;
    return false;
  default:
    assert(false);
  }
  return true;
}

expat_source::state_type
expat_source::state() const
{
  return impl_->state_;
}

unsigned
expat_source::line() const
{
  return impl_->position_.line;
}

unsigned
expat_source::column() const
{
  // For some reason, the location of the closing tag is off by one.
  unsigned column = impl_->position_.column;
  if (impl_->state_ == END && column > 0) {
    return column + 1;
  } else {
    return column + 1;
  }
}

std::string
expat_source::name() const
{
  impl_->check_state(START);
  const char *p = impl_->upcoming_.data() + impl_->elem_start_;
  return std::string(p, p + impl_->elem_len_);
}

const char *
expat_source::name_ptr() const
{
  impl_->check_state(START);
  return impl_->upcoming_.data() + impl_->elem_start_;
}

std::string
expat_source::attribute(const char *name) const
{
  size_t name_len = strlen(name);
  impl_->check_state(START);
  const char *p = impl_->upcoming_.data() + impl_->attr_start_;
  const char *end = impl_->upcoming_end();
  while (p != end && *p == ENC_ATTRIBUTE) {
    ++p;
    size_t plen = strlen(p);
    if (plen == name_len && memcmp(name, p, plen) == 0) {
      p += plen + 1;
      return std::string(p);
    }
    p += plen + 1; // key
    p += strlen(p) + 1; // value
  }
  return std::string();
}

std::map<std::string, std::string>
expat_source::attributes() const
{
  impl_->check_state(START);
  std::map<std::string, std::string> result;
  attributes(result);
  return result;
}

void
expat_source::attributes(std::map<std::string, std::string> &result) const
{
  impl_->check_state(START);
  const char *p = impl_->upcoming_.data() + impl_->attr_start_;
  const char *end = impl_->upcoming_end();
  while (p != end && *p == ENC_ATTRIBUTE) {
    ++p;
    const char *key = p;
    p += strlen(p) + 1;
    const char *value = p;
    p += strlen(p) + 1;
    result.insert(std::make_pair(std::string(key), std::string(value)));
  }
}

std::string
expat_source::text() const
{
  impl_->check_state(TEXT);
  const char *p = impl_->upcoming_.data() + impl_->elem_start_;
  return std::string(p, impl_->elem_len_);
}

const char *
expat_source::text_ptr() const
{
  impl_->check_state(TEXT);
  return impl_->upcoming_.data() + impl_->elem_start_;
}

std::string
expat_source::text_and_next()
{
  impl_->check_state(TEXT);
  const char *p = impl_->upcoming_.data() + impl_->elem_start_;
  std::string result(p, impl_->elem_len_);
  next();
  while (impl_->state_ == TEXT) {
    p = impl_->upcoming_.data() + impl_->elem_start_;
    result.append(p, impl_->elem_len_);
    next();
  }
  return result;
}

void
expat_source::skip()
{
  switch (impl_->state_) {
  case INIT:
    next();
    break;
  case START:
    {
      unsigned nested = 1;
      next();
      while (nested) {
	switch (impl_->state_) {
	case START:
	  ++nested;
	  break;
	case END:
	  --nested;
	  break;
	case TEXT:
	  break;
	case EOD:
	case INIT:
	  assert(false);
	}
	next();
      }
    }
    break;
  case TEXT:
    do {
      next();
    } while (impl_->state_ == TEXT);
    break;
  case END:
    next();
    break;
  case EOD:
    break;
  }
}

void
expat_source::unnest()
{
  if (impl_->state_ != EOD) {
    while (impl_->state_ != END) {
      skip();
    }
    next();
  }
}

const char *
expat_source::state_string(state_type e)
{
  switch (e) {
  case INIT:
    return "INIT";
  case START:
    return "START";
  case END:
    return "END";
  case TEXT:
    return "TEXT";
  case EOD:
    return "EOD";
  }
  raise<std::logic_error>("expat_source::state_string");
}

//////////////////////////////////////////////////////////////////////
// expat_source::exception

expat_source::exception::exception(const impl *p)
  : line_(p->position_.line), column_(p->position_.column)
{
}

expat_source::exception::exception(const impl *p, bool)
  : line_(XML_GetCurrentLineNumber(p->handle_.raw)),
    column_(XML_GetCurrentColumnNumber(p->handle_.raw))
{
}

expat_source::exception::~exception() throw()
{
}


const char *
expat_source::exception::what() const throw()
{
  return what_.c_str();
}

//////////////////////////////////////////////////////////////////////
// expat_source::malformed

expat_source::malformed::malformed(const impl *p, const_stringref buf)
  : exception(p, false)
{
  message_ = XML_ErrorString(XML_GetErrorCode(p->handle_.raw));
  std::stringstream msg;
  msg << line() << ':' << column() << ": error=\"" << message_ << '"';
  unsigned long long index = XML_GetCurrentByteIndex(p->handle_.raw);
  if (index >= p->consumed_bytes_) {
    index -= p->consumed_bytes_;
    size_t before_count = std::min(index, 50ULL);
    size_t after_count = std::min(buf.size() - index, 50ULL);
    buf.substr(index - before_count, before_count).str(before_ );
    buf.substr(index, after_count).str(after_);

    if (!before_.empty()) {
      msg << " before=\"" << quote(before_) << '"';
    }
    if (!after_.empty()) {
      msg << " after=\"" << quote(after_) << '"';
    }
    what_ = msg.str();
  }
}

expat_source::malformed::~malformed() throw()
{
}

//////////////////////////////////////////////////////////////////////
// expat_source::malformed

expat_source::entity_declaration::entity_declaration
  (const impl *p, const_stringref buf)
    : malformed(p, buf)
{
  // FIXME: We should refactor this using C++11 delegating
  // constructors.
  message_ = "unexpected XML entity";
  std::stringstream msg;
  msg << line() << ':' << column() << ": error=\"" << message_ << '"';
    if (!before_.empty()) {
      msg << " before=\"" << quote(before_) << '"';
    }
    if (!after_.empty()) {
      msg << " after=\"" << quote(after_) << '"';
    }
    what_ = msg.str();
}

expat_source::entity_declaration::~entity_declaration() throw()
{
}

//////////////////////////////////////////////////////////////////////
// expat_source::illegal_state

expat_source::illegal_state::illegal_state(const impl *p,
					   state_type expected)
  : exception(p)
{
  std::stringstream str;
  str << line() << ':' << column() << ": actual=" << state_string(p->state_)
      << " expected=" << state_string(expected);
  what_ = str.str();
}

expat_source::illegal_state::~illegal_state() throw ()
{
}

//////////////////////////////////////////////////////////////////////
// expat_source::source_error

expat_source::source_error::source_error(const impl *p)
  : exception(p, false)
{
  std::stringstream str;
  str << line() << ':' << column() << ": " << p->error_;
  what_ = str.str();
}

expat_source::source_error::~source_error() throw()
{
}

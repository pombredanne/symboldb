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

#include <cxxll/pg_split_statement.hpp>

#include <cstring>
#include <stdexcept>

void
cxxll::pg_split_statement(const char *sql, std::vector<std::string> &result)
{
  const char *last = sql;
  bool in_statement = false;
  while (*sql) {
    char ch = *sql;
    ++sql;
    switch (ch) {
    case '-':
      // Comment.
      if (*sql == '-') {
	sql = strchr(sql + 1, '\n');
	if (!sql) {
	  return;
	}
	++sql;
	if (!in_statement) {
	  last = sql;
	}
      }
      break;
    case '\'':
      if (!in_statement) {
	throw std::runtime_error("string at start of SQL statement");
      }
      while (*sql) {
	if (*sql == '\'') {
	  ++sql;
	  if (*sql != '\'') {
	    break;
	  }
	}
	++sql;
      }
      if (*sql == '\0') {
	throw std::runtime_error("unterminated SQL statement");
      }
      break;
    case ';':
      if (!in_statement) {
	throw std::runtime_error("empty SQL statement");
      }
      result.push_back(std::string(last, sql));
      if (result.back().empty()) {
	result.pop_back();
      }
      last = sql;
      in_statement = false;
      break;
    default:
      in_statement = in_statement || ch > ' '; // not whitespace
      if (!in_statement) {
	last = sql;
      }
    }
  }
  if (last != sql) {
    throw std::runtime_error("unterminated SQL statement");
  }
}

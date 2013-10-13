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
#include <cxxll/const_stringref.hpp>
#include <cxxll/raise.hpp>

#include <cstring>

void
cxxll::pg_split_statement(const char *sqlptr, std::vector<std::string> &result)
{
  const_stringref sql(sqlptr);
  const_stringref last = sql;
  bool in_statement = false;
  while (!sql.empty()) {
    char ch = sql[0];
    ++sql;
    switch (ch) {
    case '-':
      // Comment.
      if (!sql.empty() && sql[0] == '-') {
	sql = (sql + 1).chr('\n');
	if (sql.empty()) {
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
	raise<std::runtime_error>("string at start of SQL statement");
      }
      while (!sql.empty()) {
	if (sql[0] == '\'') {
	  ++sql;
	  if (sql.empty() || sql[0] != '\'') {
	    break;
	  }
	}
	++sql;
      }
      if (sql.empty()) {
	raise<std::runtime_error>("unterminated string in SQL statement");
      }
      break;
    case '$':
      if (!in_statement) {
	raise<std::runtime_error>("$ at start of SQL statement");
      }
      if (!sql.empty() && sql[0] == '$') {
	// $$-terminated string.
	++sql;
	while (!sql.empty()) {
	  if (sql[0] == '$') {
	    ++sql;
	    if (!sql.empty() && sql[0] == '$') {
	      ++sql;
	      break;
	    }
	  } else {
	    ++sql;
	  }
	}
	if (sql.empty()) {
	  raise<std::runtime_error>("unterminated $$ string in SQL statement");
	}
      }
      break;
    case ';':
      if (!in_statement) {
	raise<std::runtime_error>("empty SQL statement");
      }
      result.push_back(last.upto(sql).str());
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
  if (!last.empty()) {
    raise<std::runtime_error>("unterminated final SQL statement");
  }
}

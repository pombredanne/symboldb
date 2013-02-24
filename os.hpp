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

// Miscellaneous operating system interfaces.

#pragma once

#include <string>

// Returns true if PATH is a directory.
bool is_directory(const char *path);

// Returns true if PATH is executable (by the current user).
bool is_executable(const char *path);

// Returns true if PATH exists in the file system (not necessarily as
// a file).
bool path_exists(const char *path);

// Returns the path to the home directory.
std::string home_directory();

// Creates all directories in PATH, with MODE (should be 0700 or
// 0777).  Return true if the PATH is a directory (that is, if the
// hierarchy already existed or was created successfully), otherwise
// false.
bool make_directory_hierarchy(const char *path, unsigned mode);

// Creates a temporary directory with the indicated prefix (which
// shall not contain any XXXXXX characters).  Throws os_exception on
// failure.
std::string make_temporary_directory(const char *prefix);

// Removes the directory tree at ROOT.  Throws os_exception on the
// first error.
void remove_directory_tree(const char *root);

// Wrapper around getcwd(3).
std::string current_directory();

// Wrapper around readlink(2).  Throws os_exception for general errors
// and std::bad_alloc for memory allocation errors.
std::string readlink(const char *path);

// Wrapper around realpath(3).  Throws os_exception for general errors
// and std::bad_alloc for memory allocation errors.
std::string realpath(const char *path);

// Turns an errno code into a string.  Uses ERROR(nnn) for unknown
// errors.  Can throw std::bad_alloc.
std::string error_string(int);

// Turns an errno code into the matching E* constant.  Returns NULL
// for unknown constants.  Does not throw.
const char *error_string_or_null(int code) throw();

// Turns the current errno value into a string.  Can throw
// std::bad_alloc.
std::string error_string();

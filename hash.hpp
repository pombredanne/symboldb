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

#pragma once

#include <vector>
#include <string>

std::vector<unsigned char> hash_sha256(const std::vector<unsigned char> &data);

// Reads the file at PATH and writes its SHA-256 hash to DIGEST.
// ERROR is updated in case of file system errors, and the function
// returns false.  On NSS errors, and exception is thrown.
bool hash_sha256_file(const char *path, std::vector<unsigned char> &digest,
		      std::string &error);

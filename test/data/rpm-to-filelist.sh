#!/bin/bash
# Copyright (C) 2013 Red Hat, Inc.
# Written by Florian Weimer <fweimer@redhat.com>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

# Helper script to generate the .filelist files.  Note that these
# might need manual post-processing.

for rpmfile in "$@" ; do
    listfile="$(basename "$rpmfile" .rpm).filelist"
    # grep filters out non-files
    # sed removes the file type column, translates empty fields, and
    # normalizes ghost hard links and size to zero.
    rpm -qp --qf '[%{filemodes:perms},%{fileflags:fflags},%{fileinodes},%{fileusername},%{filegroupname},%{filesizes},%{filenames}\n]' "$rpmfile" \
	| grep ^- | sed -e s/^-// -e 's/,,/,"",/' -e 's/,g,[^0][0-9]*,\([^,]*,[^,]*\),[0-9]*,/,g,0,\1,0,/' \
	> "$listfile"
done


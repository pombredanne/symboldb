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

# Helper script to generate the rpm-dependencies.csv file

(
    for rpm in "$@" ; do
	source="$(rpm -qp --qf '%{sourcerpm}' "$rpm")"
	if test "$source" = "(none)" ; then
	    nevra="$(rpm -qp --qf '%{nevr}' "$rpm").src"
	else
	    nevra="$(rpm -qp --qf '%{nevra}' "$rpm")"
	fi
	for kind in require provide obsolete conflict ; do
	    rpm -qp --${kind}s "$rpm" | while read nam op cap ; do
		echo "$nevra,$kind,$nam,${op:-\"\"},${cap:-\"\"}"
	    done
	done
    done
) | LC_ALL=C sort -t, -k1,3

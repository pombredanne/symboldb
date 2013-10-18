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

set -ex

if test -z "$symboldb" ; then
    exit 11
fi

cache_dir="$(mktemp -d)"

cleanup () {
    rm -rf "$cache_dir"
}

trap cleanup 0

symboldb () {
    "$symboldb" --cache="$cache_dir" "$@"
}

psql -c "CREATE DATABASE symboldb" template1
psql -c "CREATE DATABASE symboldb1" template1
export PGDATABASE=symboldb

symboldb --create-schema-base
symboldb --load-rpm test/data/*.rpm
symboldb --create-schema-index
symboldb --expire

# Test single-step schema creation.
PGDATABASE=symboldb1 "$symboldb" --create-schema

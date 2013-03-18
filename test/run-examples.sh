#!/bin/bash

set -e

symboldb=build/symboldb

if test "$1" = test-shell ; then
    export PGDATABASE=template1
    $symboldb --create-schema
    $symboldb --create-set Fedora/18/x86_64 test/data/*.{x86_64,i686}.rpm
    exec $symboldb --run-example doc/examples/*.txt
fi
exec build/pgtestshell /bin/bash "$0" test-shell

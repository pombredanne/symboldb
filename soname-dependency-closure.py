#!/usr/bin/python3

# This is a demo implementation of the DT_NEEDED resolver because the
# initial attempt in SQL resulted in far too much data.  This Python
# implementation runs fast enough.

import psycopg2
import sys

_, package_set = sys.argv 

conn = psycopg2.connect("")

def get_package_set(name):
    cur = conn.cursor()
    cur.execute("SELECT id FROM symboldb.package_set WHERE name = %s", (name,))
    row = cur.fetchone()
    if row is None:
        raise ValueError("invalid package set name: " + repr(name))
    return row[0]

package_set = get_package_set(package_set)

def get_soname_providers(package_set):
    "Returns a dict from architecture to a nested dict of sonames to file IDs."
    cur = conn.cursor()
    cur.execute("SELECT arch, soname, files[1] FROM symboldb.elf_soname_provider WHERE package_set = %s",
                (package_set,))
    result = {}
    for (arch, soname, file) in cur.fetchall():
        try:
            archdict = result[arch]
        except KeyError:
            archdict = {}
            result[arch] = archdict
        archdict[soname] = file
    return result

def get_elf_files(package_set):
    "Returns a list of pairs (file ID, architecture)."
    cur = conn.cursor()
    cur.execute("""SELECT ef.file, f.name, ef.arch
FROM symboldb.elf_file ef
JOIN symboldb.file f ON ef.file = f.id
JOIN symboldb.package_set_member psm ON f.package = psm.package
WHERE psm.set = %s""", (package_set,))
    return cur.fetchall()

def get_elf_needs(package_set):
    "Returns a dict from file IDs to sets of sonames."
    cur = conn.cursor()
    cur.execute("""SELECT en.file, en.name
FROM symboldb.elf_needed en
JOIN symboldb.file f ON en.file = f.id
JOIN symboldb.package_set_member psm ON f.package = psm.package
WHERE psm.set = %s""", (package_set,))
    result = {}
    for (fid, needed) in cur.fetchall():
        try:
            fidset = result[fid]
        except KeyError:
            fidset = set()
            result[fid] = fidset
        fidset.add(needed)
    return result

soname_providers = get_soname_providers(package_set)
elf_needs = get_elf_needs(package_set)
elf_files = {}
for (file, path, arch) in get_elf_files(package_set):
    sonames = soname_providers[arch]
    closure = set()
    for so in elf_needs.get(file, ()):
        try:
            closure.add(sonames[so])
        except KeyError:
            print("unknown soname {0} referenced from {1}".format(
                    so, path))
    changed = True
    while changed:
        changed = False
        to_add = set()
        for f in closure:
            for newso in elf_needs.get(f, ()):
                newf = sonames[newso]
                if newf not in closure:
                    to_add.add(newf)
                    changed = True
        closure.update(to_add)
    elf_files[file] = closure
print(elf_files)

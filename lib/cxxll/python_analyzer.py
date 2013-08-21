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

# This file must be valid Python 2 and Python 3, so that we can use it
# to analyze both language variants.  See python_imports.cpp for a
# description of the protocol.

import struct
import sys
import ast

try:
    instream = sys.stdin.buffer # Python 3
    outstream = sys.stdout.buffer
except AttributeError:
    instream = sys.stdin # Python 2
    outstream = sys.stdout

try:
    unicodetype = unicode # Python 2
except NameError:
    unicodetype = str # Python 3

def read_number():
    return struct.unpack(">I", instream.read(4))[0]

def read_string():
    return instream.read(read_number())

def write_number(n):
    outstream.write(struct.pack(">I", n))

def write_string(s):
    if type(s) == unicodetype:
        s = s.encode("UTF-8")
    write_number(len(s))
    outstream.write(s)

def write_array(l):
    write_number(len(l))
    for e in l:
        write_string(e)

class ImportVisitor(ast.NodeVisitor):
    def __init__(self):
        super(ImportVisitor, self).__init__()
        self.imports = []
        self.attributes = set()
        self.functions = set()
    def visit_Import(self, node):
        for alias in node.names:
            self.imports.append(alias.name)
    def visit_ImportFrom(self, node):
        dots = "." * node.level
        if node.module:
            module = dots + node.module + "."
        else:
            module = dots
        for alias in node.names:
            self.imports.append(module + alias.name)
    def visit_Attribute(self, node):
        self.attributes.add(node.attr)
        self.generic_visit(node)
    def visit_FunctionDef(self, node):
        self.functions.add(node.name)
        self.generic_visit(node)

while True:
    source = read_string()
    try:
        parsed = ast.parse(source)
    except SyntaxError as e:
        write_string(e.msg)
        write_number(e.lineno)
        write_number(0) # imports
        write_number(0) # attributes
        write_number(0) # functions
        outstream.flush()
        continue
    v = ImportVisitor()
    v.visit(parsed)

    write_string(b"") # error message
    write_number(0) # error line number
    write_array(v.imports)
    write_array(sorted(v.attributes))
    write_array(sorted(v.functions))
    outstream.flush()

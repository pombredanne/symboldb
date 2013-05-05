-- Copyright (C) 2012, 2013 Red Hat, Inc.
-- Written by Florian Weimer <fweimer@redhat.com>
--
-- This program is free software: you can redistribute it and/or modify
-- it under the terms of the GNU General Public License as published by
-- the Free Software Foundation, either version 3 of the License, or
-- (at your option) any later version.
--
-- This program is distributed in the hope that it will be useful,
-- but WITHOUT ANY WARRANTY; without even the implied warranty of
-- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
-- GNU General Public License for more details.
--
-- You should have received a copy of the GNU General Public License
-- along with this program.  If not, see <http://www.gnu.org/licenses/>.

-- This file contains the indexes not essential for loading.

CREATE INDEX ON symboldb.package (name, version);
CREATE INDEX ON symboldb.package (symboldb.nvra(package));
CREATE INDEX ON symboldb.package (symboldb.nevra(package));

CREATE INDEX ON symboldb.package_digest (package_id);

CREATE INDEX ON symboldb.package_require (package_id);
CREATE INDEX ON symboldb.package_require (capability);

CREATE INDEX ON symboldb.package_provide (package_id);
CREATE INDEX ON symboldb.package_provide (capability);

CREATE INDEX ON symboldb.package_set_member (set_id);
CREATE INDEX ON symboldb.package_set_member (package_id);

ALTER TABLE symboldb.file ADD UNIQUE (package_id, name);
CREATE INDEX ON symboldb.file (name);
CREATE INDEX ON symboldb.file (contents_id);

ALTER TABLE symboldb.symlink ADD PRIMARY KEY (package_id, name);
CREATE INDEX ON symboldb.symlink (name);
CREATE INDEX ON symboldb.symlink (target);

ALTER TABLE symboldb.directory ADD PRIMARY KEY (package_id, name);
CREATE INDEX ON symboldb.directory (name);

CREATE INDEX ON symboldb.elf_definition (contents_id);
CREATE INDEX ON symboldb.elf_definition (name, version);

CREATE INDEX ON symboldb.elf_reference (contents_id);
CREATE INDEX ON symboldb.elf_reference (name, version);

ALTER TABLE symboldb.elf_needed ADD PRIMARY KEY (contents_id, name);
CREATE INDEX ON symboldb.elf_needed (name);

ALTER TABLE symboldb.elf_rpath ADD PRIMARY KEY (contents_id, path);

ALTER TABLE symboldb.elf_runpath ADD PRIMARY KEY (contents_id, path);

CREATE INDEX ON symboldb.elf_closure (file_id);
CREATE INDEX ON symboldb.elf_closure (needed);

CREATE INDEX ON symboldb.java_class (name);
CREATE INDEX ON symboldb.java_class (super_class);

ALTER TABLE symboldb.java_interface ADD  PRIMARY KEY (class_id, name);
CREATE INDEX ON symboldb.java_interface (name);

ALTER TABLE symboldb.java_class_reference ADD PRIMARY KEY (name, class_id);
CREATE INDEX ON symboldb.java_class_reference (class_id);

CREATE INDEX ON symboldb.java_error (contents_id, path);

ALTER TABLE symboldb.java_class_contents ADD PRIMARY KEY (contents_id, class_id);
CREATE INDEX ON symboldb.java_class_contents (class_id);
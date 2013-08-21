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

CREATE EXTENSION IF NOT EXISTS pg_trgm;

CREATE SCHEMA symboldb;

CREATE TYPE symboldb.elf_arch AS ENUM (
  'i386', 'x86_64',
  'ppc', 'ppc64',
  's390', 's390x',
  'sparc', 'sparc64'
);

CREATE TYPE symboldb.rpm_kind AS ENUM ('binary', 'source');

CREATE TYPE symboldb.elf_visibility AS ENUM
  ('default', 'internal', 'hidden', 'protected');

CREATE DOMAIN symboldb.elf_byte AS SMALLINT
  CHECK (VALUE BETWEEN 0 AND 255);
CREATE DOMAIN symboldb.elf_short AS INTEGER
  CHECK (VALUE BETWEEN 0 AND 65536);

CREATE DOMAIN symboldb.elf_symbol_type AS SMALLINT
  CHECK (VALUE BETWEEN 0 AND 15);
CREATE DOMAIN symboldb.elf_binding_type AS SMALLINT
  CHECK (VALUE BETWEEN 0 AND 15);

-- The actual value is unsigned, but we want to save space.
CREATE DOMAIN symboldb.elf_section_type AS SMALLINT;

CREATE TABLE symboldb.package (
  package_id SERIAL NOT NULL PRIMARY KEY,
  kind symboldb.rpm_kind NOT NULL,
  epoch INTEGER CHECK (epoch >= 0),
  name TEXT NOT NULL CHECK (LENGTH(name) > 0) COLLATE "C",
  version TEXT NOT NULL CHECK (LENGTH(version) > 0),
  release TEXT NOT NULL CHECK (LENGTH(release) > 0),
  arch TEXT NOT NULL CHECK (LENGTH(arch) > 0),
  hash BYTEA NOT NULL UNIQUE CHECK (LENGTH(hash) = 20),
  source TEXT CHECK (LENGTH(source) > 0) COLLATE "C",
  build_host TEXT NOT NULL CHECK (LENGTH(build_host) > 0) COLLATE "C",
  build_time TIMESTAMP(0) WITHOUT TIME ZONE NOT NULL,
  summary TEXT NOT NULL COLLATE "C",
  description TEXT NOT NULL COLLATE "C",
  license TEXT NOT NULL COLLATE "C",
  rpm_group TEXT NOT NULL COLLATE "C",
  normalized BOOLEAN NOT NULL
);
COMMENT ON COLUMN symboldb.package.source IS
  'file name of the source RPM package';
COMMENT ON COLUMN symboldb.package.hash IS
  'internal SHA1HEADER hash of the RPM contents';
COMMENT ON COLUMN symboldb.package.normalized IS
  'indicates that a textual header has been forced to UTF-8 encoding';

CREATE FUNCTION symboldb.arch_src (symboldb.package) RETURNS TEXT AS $$
  SELECT CASE WHEN $1.kind = 'source' THEN 'src'
    ELSE $1.arch::text
  END;
$$ IMMUTABLE STRICT LANGUAGE SQL;

CREATE FUNCTION symboldb.nvra (symboldb.package) RETURNS TEXT AS $$
  SELECT
    $1.name || '-' || $1.version || '-' || $1.release || '.'
      || symboldb.arch_src($1)
  ;
$$ IMMUTABLE STRICT LANGUAGE SQL;

CREATE FUNCTION symboldb.nevra (symboldb.package) RETURNS TEXT AS $$
  SELECT
    CASE
      WHEN $1.epoch IS NULL THEN symboldb.nvra ($1)
      ELSE $1.name || '-' || $1.epoch || ':' || $1.version
        || '-' || $1.release || '.' || symboldb.arch_src($1)
    END
  ;
$$ IMMUTABLE STRICT LANGUAGE SQL;

CREATE TABLE symboldb.package_digest (
  digest BYTEA NOT NULL PRIMARY KEY
    CHECK (LENGTH(digest) IN (20, 32)),
  length BIGINT NOT NULL CHECK (length > 0),
  package_id INTEGER NOT NULL
    REFERENCES symboldb.package ON DELETE CASCADE
);
COMMENT ON table symboldb.package_digest IS
  'SHA-1 and SHA-256 hashes of multiple representations of the same RPM package';

CREATE TABLE symboldb.package_url (
  package_id INTEGER NOT NULL
    REFERENCES symboldb.package ON DELETE CASCADE,
  url TEXT NOT NULL CHECK (LENGTH(url) > 0) COLLATE "C",
  PRIMARY KEY (package_id, url)
);
COMMENT ON TABLE symboldb.package_url IS
  'download source for a package';

CREATE FUNCTION symboldb.add_package_url (INTEGER, TEXT) RETURNS BOOLEAN
  LANGUAGE 'plpgsql' AS $$
DECLARE
  dummy BOOLEAN;
BEGIN
  SELECT TRUE INTO dummy FROM symboldb.package_url
    WHERE package_id = $1 AND url = $2;
  IF FOUND THEN
    RETURN FALSE;
  END IF;
  INSERT INTO symboldb.package_url (package_id, url) VALUES ($1, $2);
  RETURN TRUE;
END;
$$;


CREATE TABLE symboldb.package_require (
  package_id INTEGER NOT NULL
    REFERENCES symboldb.package ON DELETE CASCADE,
  capability TEXT NOT NULL COLLATE "C",
  op TEXT COLLATE "C",
  version TEXT COLLATE "C",
  pre BOOLEAN NOT NULL,
  build BOOLEAN NOT NULL
);

CREATE TABLE symboldb.package_provide (
  package_id INTEGER NOT NULL
    REFERENCES symboldb.package ON DELETE CASCADE,
  capability TEXT NOT NULL COLLATE "C",
  op TEXT COLLATE "C",
  version TEXT COLLATE "C",
  pre BOOLEAN NOT NULL,
  build BOOLEAN NOT NULL
);

CREATE TABLE symboldb.package_obsolete (
  package_id INTEGER NOT NULL
    REFERENCES symboldb.package ON DELETE CASCADE,
  capability TEXT NOT NULL COLLATE "C",
  op TEXT COLLATE "C",
  version TEXT COLLATE "C",
  pre BOOLEAN NOT NULL,
  build BOOLEAN NOT NULL
);

CREATE TABLE symboldb.package_set (
  set_id SERIAL NOT NULL PRIMARY KEY,
  name TEXT NOT NULL UNIQUE COLLATE "C"
);

CREATE TABLE symboldb.package_set_member (
  set_id INTEGER NOT NULL
    REFERENCES symboldb.package_set ON DELETE CASCADE,
  package_id INTEGER NOT NULL REFERENCES symboldb.package
);

CREATE TABLE symboldb.file_contents (
  contents_id SERIAL NOT NULL PRIMARY KEY,
  mode INTEGER NOT NULL CHECK (mode >= 0),
  length BIGINT NOT NULL CHECK(length >= 0),
  flags INTEGER NOT NULL,
  user_name TEXT NOT NULL CHECK (LENGTH(user_name) > 0) COLLATE "C",
  group_name TEXT NOT NULL CHECK (LENGTH(group_name) > 0) COLLATE "C",
  digest BYTEA NOT NULL CHECK (LENGTH(digest) = 32),
  contents BYTEA NOT NULL,
  row_hash BYTEA NOT NULL UNIQUE CHECK (LENGTH(row_hash) = 16)
);
COMMENT ON COLUMN symboldb.file_contents.flags IS
  'from the FILEFLAGS header of the RPM file; indicates ghost status etc.';
COMMENT ON COLUMN symboldb.file_contents.digest IS
  'SHA-256 digest of the entire file contents';
COMMENT ON COLUMN symboldb.file_contents.contents IS
  'preview of the file contents';
COMMENT ON COLUMN symboldb.file_contents.row_hash IS
  'internal hash used for deduplication';
CREATE INDEX ON symboldb.file_contents (digest);

CREATE FUNCTION symboldb.intern_file_contents (
  row_hash BYTEA, length BIGINT, mode INTEGER,  flags INTEGER,
  user_name TEXT, group_name TEXT, digest BYTEA, contents BYTEA,
  OUT cid INTEGER, OUT added BOOLEAN, OUT contents_length INTEGER
) LANGUAGE 'plpgsql' AS $$
BEGIN
  SELECT contents_id, LENGTH(fc.contents) INTO cid, contents_length
    FROM symboldb.file_contents fc WHERE fc.row_hash = $1;
  IF FOUND THEN
    added := FALSE;
    RETURN;
  END IF;
  INSERT INTO symboldb.file_contents
     (row_hash, length, mode, flags, user_name, group_name, digest, contents)
     VALUES ($1, $2, $3, $4, $5, $6, $7, $8) RETURNING contents_id INTO cid;
  added := TRUE;
  contents_length := LENGTH(contents);
  RETURN;
END;
$$;

CREATE TABLE symboldb.file (
  file_id SERIAL NOT NULL PRIMARY KEY,
  package_id INTEGER NOT NULL
    REFERENCES symboldb.package ON DELETE CASCADE,
  contents_id INTEGER NOT NULL REFERENCES symboldb.file_contents,
  inode INTEGER NOT NULL,
  mtime NUMERIC NOT NULL CHECK (mtime >= 0),
  name TEXT NOT NULL CHECK (LENGTH(name) > 0) COLLATE "C",
  normalized BOOLEAN NOT NULL
);
COMMENT ON COLUMN symboldb.file.normalized IS
  'indicates that the file name has been forced to UTF-8 encoding';
COMMENT ON COLUMN symboldb.file.inode IS
  'intra-package inode number (used to indicate hardlinks)';

CREATE FUNCTION symboldb.add_file (
  row_hash BYTEA, length BIGINT, mode INTEGER, flags INTEGER,
  user_name TEXT, group_name TEXT, digest BYTEA, contents BYTEA,
  package_id INTEGER, inode INTEGER, mtime INTEGER,
  name TEXT, normalized BOOLEAN,
  OUT fid INTEGER, OUT cid INTEGER, OUT added BOOLEAN,
  OUT contents_length INTEGER
) LANGUAGE 'plpgsql' AS $$
BEGIN
  SELECT ifc.cid, ifc.added, ifc.contents_length
    INTO cid, added, contents_length
    FROM symboldb.intern_file_contents
      ($1, $2, $3, $4, $5, $6, $7, $8) ifc;
  INSERT INTO symboldb.file VALUES
    (DEFAULT, $9, cid, $10, $11, $12, $13) RETURNING file_id INTO fid;
END;
$$;

CREATE TABLE symboldb.symlink (
  package_id INTEGER NOT NULL
    REFERENCES symboldb.package ON DELETE CASCADE,
  flags INTEGER NOT NULL,
  name TEXT NOT NULL CHECK (LENGTH(name) > 0) COLLATE "C",
  target TEXT NOT NULL CHECK (LENGTH(target) > 0) COLLATE "C",
  user_name TEXT NOT NULL CHECK (LENGTH(user_name) > 0) COLLATE "C",
  group_name TEXT NOT NULL CHECK (LENGTH(group_name) > 0) COLLATE "C",
  mtime NUMERIC NOT NULL CHECK (mtime >= 0),
  normalized BOOLEAN NOT NULL
);
COMMENT ON COLUMN symboldb.symlink.normalized IS
  'indicates that the symlink name has been forced to UTF-8 encoding';
COMMENT ON COLUMN symboldb.symlink.flags IS
  'from the FILEFLAGS header of the RPM; indicates ghost status etc.';

CREATE TABLE symboldb.directory (
  package_id INTEGER NOT NULL
    REFERENCES symboldb.package ON DELETE CASCADE,
  flags INTEGER NOT NULL,
  name TEXT NOT NULL CHECK (LENGTH(name) > 0) COLLATE "C",
  user_name TEXT NOT NULL CHECK (LENGTH(user_name) > 0) COLLATE "C",
  group_name TEXT NOT NULL CHECK (LENGTH(group_name) > 0) COLLATE "C",
  mtime NUMERIC NOT NULL CHECK (mtime >= 0),
  mode INTEGER NOT NULL CHECK (mode >= 0),
  normalized BOOLEAN NOT NULL
);
COMMENT ON COLUMN symboldb.directory.normalized IS
  'indicates that the directory name has been forced to UTF-8 encoding';
COMMENT ON COLUMN symboldb.directory.flags IS
  'from the FILEFLAGS header of the RPM; indicates ghost status etc.';

CREATE TABLE symboldb.elf_file (
  contents_id INTEGER NOT NULL PRIMARY KEY
    REFERENCES symboldb.file_contents ON DELETE CASCADE,
  ei_class symboldb.elf_byte NOT NULL,
  ei_data symboldb.elf_byte NOT NULL,
  e_type symboldb.elf_short NOT NULL,
  e_machine symboldb.elf_short NOT NULL,
  arch symboldb.elf_arch,
  soname TEXT COLLATE "C",
  interp TEXT COLLATE "C",
  build_id BYTEA CHECK (LENGTH(build_id) > 0)
);

CREATE TABLE symboldb.elf_program_header (
  contents_id INTEGER NOT NULL
    REFERENCES symboldb.file_contents ON DELETE CASCADE,
  type int8 NOT NULL,
  file_offset int8 NOT NULL,
  virt_addr int8 NOT NULL,
  phys_addr int8 NOT NULL,
  file_size int8 NOT NULL,
  memory_size int8 NOT NULL,
  align INTEGER NOT NULL,
  readable BOOLEAN NOT NULL,
  writable BOOLEAN NOT NULL,
  executable BOOLEAN NOT NULL
);

CREATE TABLE symboldb.elf_definition (
  contents_id INTEGER NOT NULL
    REFERENCES symboldb.file_contents ON DELETE CASCADE,
  name TEXT NOT NULL CHECK(length(name) > 0) COLLATE "C",
  version TEXT CHECK (LENGTH(version) > 0) COLLATE "C",
  primary_version BOOLEAN NOT NULL,
  symbol_type symboldb.elf_symbol_type NOT NULL,
  binding symboldb.elf_binding_type NOT NULL,
  section symboldb.elf_section_type NOT NULL,
  xsection INTEGER,
  visibility symboldb.elf_visibility NOT NULL,
  CHECK (CASE WHEN version IS NULL THEN NOT primary_version ELSE TRUE END),
  -- xsection is only present when section is SHN_XINDEX.
  CHECK ((xsection IS NOT NULL) = (section = -1))
);

CREATE TABLE symboldb.elf_reference (
  contents_id INTEGER NOT NULL
    REFERENCES symboldb.file_contents ON DELETE CASCADE,
  name TEXT NOT NULL CHECK(length(name) > 0) COLLATE "C",
  version TEXT CHECK (LENGTH(version) > 0) COLLATE "C",
  symbol_type symboldb.elf_symbol_type NOT NULL,
  binding symboldb.elf_binding_type NOT NULL,
  visibility symboldb.elf_visibility NOT NULL
);

CREATE TABLE symboldb.elf_needed (
  contents_id INTEGER NOT NULL
    REFERENCES symboldb.file_contents ON DELETE CASCADE,
  name TEXT NOT NULL COLLATE "C"
);

CREATE TABLE symboldb.elf_rpath (
  contents_id INTEGER NOT NULL
    REFERENCES symboldb.file_contents ON DELETE CASCADE,
  path TEXT NOT NULL COLLATE "C"
);

CREATE TABLE symboldb.elf_runpath (
  contents_id INTEGER NOT NULL
    REFERENCES symboldb.file_contents ON DELETE CASCADE,
  path TEXT NOT NULL COLLATE "C"
);

CREATE TABLE symboldb.elf_dynamic (
  contents_id INTEGER NOT NULL
    REFERENCES symboldb.file_contents ON DELETE CASCADE,
  tag int8 NOT NULL,
  value int8 NOT NULL
);

CREATE TABLE symboldb.elf_error (
  contents_id INTEGER NOT NULL
    REFERENCES symboldb.file_contents ON DELETE CASCADE,
  message TEXT
);

CREATE TABLE symboldb.elf_closure (
  set_id INTEGER NOT NULL REFERENCES symboldb.package_set
    ON DELETE CASCADE,
  file_id INTEGER NOT NULL REFERENCES symboldb.file,
  needed INTEGER NOT NULL REFERENCES symboldb.file
);

-- Java classes.

CREATE TABLE symboldb.java_class (
  class_id SERIAL NOT NULL PRIMARY KEY,
  access_flags INTEGER NOT NULL CHECK (access_flags BETWEEN 0 AND 65536),
  name TEXT NOT NULL CHECK(LENGTH(name) > 0) COLLATE "C",
  digest BYTEA NOT NULL UNIQUE CHECK(LENGTH(digest) = 32),
  super_class TEXT NOT NULL COLLATE "C"
);

CREATE FUNCTION symboldb.intern_java_class (
  digest BYTEA, name TEXT, super_class TEXT, access_flags INTEGER,
  OUT cid INTEGER, OUT added BOOLEAN
) LANGUAGE plpgsql AS $$
BEGIN
  SELECT class_id INTO cid
    FROM symboldb.java_class jc WHERE jc.digest = $1;
  IF FOUND THEN
    added := FALSE;
    RETURN;
  END IF;
  INSERT INTO symboldb.java_class (digest, name, super_class, access_flags)
     VALUES ($1, $2, $3, $4) RETURNING class_id INTO cid;
  added := TRUE;
  RETURN;
END;
$$;

CREATE TABLE symboldb.java_interface (
  class_id INTEGER NOT NULL REFERENCES symboldb.java_class ON DELETE CASCADE,
  name TEXT NOT NULL CHECK(LENGTH(name) > 0) COLLATE "C"
);

CREATE TABLE symboldb.java_class_reference (
  class_id INTEGER NOT NULL REFERENCES symboldb.java_class ON DELETE CASCADE,
  name TEXT NOT NULL CHECK (LENGTH(name) > 0) COLLATE "C"
);

CREATE TABLE symboldb.java_error (
  contents_id INTEGER NOT NULL
    REFERENCES symboldb.file_contents ON DELETE CASCADE,
  message TEXT NOT NULL CHECK (LENGTH(message) > 0),
  path TEXT NOT NULL COLLATE "C"
);

CREATE TABLE symboldb.java_class_contents (
  contents_id INTEGER NOT NULL
    REFERENCES symboldb.file_contents ON DELETE cascade,
  class_id INTEGER NOT NULL REFERENCES symboldb.java_class ON DELETE CASCADE
);

-- Python modules.

CREATE TABLE symboldb.python_import (
  contents_id INTEGER NOT NULL
    REFERENCES symboldb.file_contents ON DELETE CASCADE,
  name TEXT NOT NULL CHECK (LENGTH(name) > 0) COLLATE "C"
);
COMMENT ON COLUMN symboldb.python_import.name IS
  'qualified of the imported module/symbol, with leading . for relative import';
CREATE INDEX ON symboldb.python_import (contents_id);

CREATE TABLE symboldb.python_attribute (
  contents_id INTEGER NOT NULL
    REFERENCES symboldb.file_contents ON DELETE CASCADE,
  name TEXT NOT NULL CHECK (LENGTH(name) > 0) COLLATE "C"
);
COMMENT ON COLUMN symboldb.python_attribute.name IS
  'extracted attribute reference';
CREATE INDEX ON symboldb.python_attribute (contents_id);

CREATE TABLE symboldb.python_error (
  contents_id INTEGER NOT NULL
    REFERENCES symboldb.file_contents ON DELETE CASCADE,
  line INTEGER CHECK (line > 0),
  message TEXT NOT NULL CHECK (LENGTH(message) > 0) COLLATE "C"
);

-- URL cache (mainly for raw repository metadata).

CREATE TABLE symboldb.url_cache (
  url TEXT NOT NULL PRIMARY KEY CHECK (url LIKE '%:%') COLLATE "C",
  http_time BIGINT NOT NULL,
  data BYTEA NOT NULL,
  last_change TIMESTAMP WITHOUT TIME ZONE NOT NULL,
  last_access TIMESTAMP WITHOUT TIME ZONE NOT NULL
);
COMMENT ON TABLE symboldb.url_cache IS 'cache for URL downloads';

-- Formatting file modes.

CREATE FUNCTION symboldb.file_mode_internal
  (mode INTEGER, mask INTEGER, ch TEXT) RETURNS TEXT AS $$
  SELECT CASE WHEN ($1 & $2) <> 0 THEN $3 ELSE '-' END;
$$ IMMUTABLE STRICT LANGUAGE SQL;
COMMENT ON FUNCTION symboldb.file_mode_internal
  (INTEGER, INTEGER, TEXT) IS 'internal helper function';

CREATE FUNCTION symboldb.file_mode_internal
  (mode INTEGER, mask1 INTEGER, mask2 INTEGER,
   ch1 TEXT, ch2 TEXT, ch3 TEXT) RETURNS TEXT AS $$
  SELECT CASE
    WHEN ($1 & $2) <> 0 AND ($1 & $3) = 0 THEN $4
    WHEN ($1 & $2) = 0 AND ($1 & $3) <> 0 THEN $5
    WHEN ($1 & $2) <> 0 AND ($1 & $3) <> 0 THEN $6
    ELSE '-'
  END;
$$ IMMUTABLE STRICT LANGUAGE SQL;
COMMENT ON FUNCTION symboldb.file_mode_internal
  (INTEGER, INTEGER, INTEGER, TEXT, TEXT, TEXT) IS
  'internal helper function';

CREATE FUNCTION symboldb.file_mode (INTEGER) RETURNS TEXT AS $$
  SELECT symboldb.file_mode_internal($1, 256, 'r')
    || symboldb.file_mode_internal($1, 128, 'w')
    || symboldb.file_mode_internal($1, 64, 2048, 'x', 'S', 's')
    || symboldb.file_mode_internal($1, 32, 'r')
    || symboldb.file_mode_internal($1, 16, 'w')
    || symboldb.file_mode_internal($1, 8, 1024, 'x', 'S', 's')
    || symboldb.file_mode_internal($1, 4, 'r')
    || symboldb.file_mode_internal($1, 2, 'w')
    || symboldb.file_mode_internal($1, 1, 512, 'x', 'T', 't');
$$ IMMUTABLE STRICT LANGUAGE SQL;
COMMENT ON FUNCTION symboldb.file_mode (INTEGER) IS
  'format the integer as a file permission string';

-- Convenience functions.

CREATE FUNCTION symboldb.package_set (TEXT) RETURNS INTEGER AS $$
  SELECT set_id FROM symboldb.package_set WHERE name = $1
$$ STABLE STRICT LANGUAGE SQL;
COMMENT ON FUNCTION symboldb.file_mode (INTEGER) IS
  'return the ID of the named package set';

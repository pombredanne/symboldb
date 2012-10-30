\set ON_ERROR_STOP
BEGIN;

CREATE SCHEMA symboldb;

CREATE TYPE symboldb.arch AS ENUM
  ('noarch', 'i686', 'x86_64', 'ppc', 'ppc64', 's390', 's390x');

CREATE DOMAIN symboldb.elf_byte AS SMALLINT
  CHECK (VALUE BETWEEN 0 AND 255);
CREATE DOMAIN symboldb.elf_short AS INTEGER
  CHECK (VALUE BETWEEN 0 AND 65536);

CREATE TABLE symboldb.package (
  id SERIAL NOT NULL PRIMARY KEY,
  name TEXT NOT NULL CHECK (LENGTH(name) > 0),
  epoch INTEGER CHECK (epoch >= 0),
  version TEXT NOT NULL CHECK (LENGTH(version) > 0),
  release TEXT NOT NULL CHECK (LENGTH(release) > 0),
  arch symboldb.arch NOT NULL,
  hash BYTEA NOT NULL UNIQUE CHECK (LENGTH(hash) = 20),
  source TEXT NOT NULL CHECK (LENGTH(source) > 0)
);
COMMENT ON COLUMN symboldb.package.source IS
  'file name of the source RPM package';
CREATE INDEX ON symboldb.package (name, version);

CREATE FUNCTION symboldb.nvra (symboldb.package) RETURNS TEXT AS $$
  SELECT
    $1.name || '-' || $1.version || '-' || $1.release || '.' || $1.arch
  ;
$$ IMMUTABLE STRICT LANGUAGE SQL;

CREATE FUNCTION symboldb.nevra (symboldb.package) RETURNS TEXT AS $$
  SELECT
    CASE
      WHEN $1.epoch IS NULL THEN symboldb.nvra ($1)
      ELSE $1.name || '-' || $1.epoch || ':' || $1.version
        || '-' || $1.release || '.' || $1.arch
    END
  ;
$$ IMMUTABLE STRICT LANGUAGE SQL;

CREATE INDEX ON symboldb.package (symboldb.nvra(package));
CREATE INDEX ON symboldb.package (symboldb.nevra(package));

CREATE TABLE symboldb.package_set (
  id SERIAL NOT NULL PRIMARY KEY,
  name TEXT NOT NULL UNIQUE,
  arch TEXT NOT NULL CHECK (LENGTH(arch) > 0)
);
COMMENT ON COLUMN symboldb.package_set.arch IS 'main architecture';

CREATE TABLE symboldb.package_set_member (
  set INTEGER NOT NULL
    REFERENCES symboldb.package_set ON DELETE CASCADE,
  package INTEGER NOT NULL REFERENCES symboldb.package
);
CREATE INDEX ON symboldb.package_set_member (set);
CREATE INDEX ON symboldb.package_set_member (package);

CREATE TABLE symboldb.file (
  id SERIAL NOT NULL PRIMARY KEY,
  package INTEGER NOT NULL
    REFERENCES symboldb.package (id) ON DELETE CASCADE,
  name TEXT NOT NULL CHECK (LENGTH(name) > 0),
  user_name TEXT NOT NULL CHECK (LENGTH(user_name) > 0),
  group_name TEXT NOT NULL CHECK (LENGTH(group_name) > 0),
  mtime NUMERIC NOT NULL CHECK (mtime >= 0),
  mode INTEGER NOT NULL CHECK (mode >= 0)
);
CREATE INDEX ON symboldb.file (package);
CREATE INDEX ON symboldb.file (name);

CREATE TABLE symboldb.elf_file (
  file INTEGER NOT NULL PRIMARY KEY
    REFERENCES symboldb.file (id) ON DELETE CASCADE,
  ei_class symboldb.elf_byte NOT NULL,
  ei_data symboldb.elf_byte NOT NULL,
  e_machine symboldb.elf_short NOT NULL,
  arch symboldb.arch NOT NULL,
  soname TEXT NOT NULL
);
CREATE INDEX ON symboldb.elf_file (soname);

CREATE TABLE symboldb.elf_definition (
  file INTEGER NOT NULL
    REFERENCES symboldb.file (id) ON DELETE CASCADE,
  name TEXT NOT NULL CHECK(length(name) > 0),
  version TEXT CHECK (LENGTH(version) > 0),
  primary_version BOOLEAN NOT NULL,
  CHECK (CASE WHEN version IS NULL THEN NOT primary_version ELSE TRUE END)
);
CREATE INDEX ON symboldb.elf_definition (file);
CREATE INDEX ON symboldb.elf_definition (name, version);

CREATE TABLE symboldb.elf_reference (
  file INTEGER NOT NULL
    REFERENCES symboldb.file (id) ON DELETE CASCADE,
  name TEXT NOT NULL CHECK(length(name) > 0),
  version TEXT CHECK (LENGTH(version) > 0)
);
CREATE INDEX ON symboldb.elf_reference (file);
CREATE INDEX ON symboldb.elf_reference (name, version);

CREATE TABLE symboldb.elf_needed (
  file INTEGER NOT NULL
    REFERENCES symboldb.file (id) ON DELETE CASCADE,
  name TEXT NOT NULL,
  PRIMARY KEY (file, name)
);
CREATE INDEX ON symboldb.elf_needed (name);

CREATE TABLE symboldb.elf_rpath (
  file INTEGER NOT NULL
    REFERENCES symboldb.file (id) ON DELETE CASCADE,
  path TEXT NOT NULL,
  PRIMARY KEY (file, path)
);

CREATE TABLE symboldb.elf_runpath (
  file INTEGER NOT NULL
    REFERENCES symboldb.file (id) ON DELETE CASCADE,
  path TEXT NOT NULL,
  PRIMARY KEY (file, path)
);

CREATE TABLE symboldb.elf_error (
  file INTEGER NOT NULL
    REFERENCES symboldb.file (id) ON DELETE CASCADE,
  message TEXT
);

CREATE VIEW symboldb.elf_soname_provider AS
  SELECT set AS package_set, arch, soname,
    (SELECT array_agg(id ORDER BY LENGTH(name), name)
     FROM symboldb.file WHERE id = ANY(files)) AS files
    -- heuristic: prefer files with sorter paths
   FROM (SELECT psm.set, ef.arch, ef.soname, array_agg(DISTINCT f.id) AS files
    FROM symboldb.package_set_member psm
    JOIN symboldb.package p ON psm.package = p.id
    JOIN symboldb.file f ON p.id = f.package
    JOIN symboldb.elf_file ef ON f.id = ef.file
    WHERE EXISTS(SELECT 1
      FROM symboldb.package_set_member psm2
      JOIN symboldb.package p2 ON psm2.package = p2.id
      JOIN symboldb.file f2 ON p2.id = f2.package
      JOIN symboldb.elf_file ef2 ON f2.id = ef2.file
      JOIN symboldb.elf_needed en2 ON f2.id = en2.file
      WHERE psm.set = psm2.set
      AND ef.arch = ef2.arch AND ef.soname = en2.name)
    GROUP BY psm.set, ef.arch, ef.soname
  ) x;
COMMENT ON VIEW symboldb.elf_soname_provider IS
  'files which provide a ELF soname for a specific architecture within a package set';

CREATE VIEW symboldb.elf_soname_needed_missing AS
  SELECT psm.set AS package_set, ef.arch, en.file, en.name AS soname
      FROM symboldb.package_set_member psm
      JOIN symboldb.package p ON psm.package = p.id
      JOIN symboldb.file f ON p.id = f.package
      JOIN symboldb.elf_file ef ON f.id = ef.file
      JOIN symboldb.elf_needed en ON f.id = en.file
    WHERE NOT EXISTS (SELECT 1
      FROM symboldb.elf_soname_provider esp
      WHERE esp.package_set = psm.set
      AND esp.arch = ef.arch AND esp.soname = en.name);
COMMENT ON VIEW symboldb.elf_soname_provider IS
  'files which need a soname which cannot be resolved within the same package set';

COMMIT;

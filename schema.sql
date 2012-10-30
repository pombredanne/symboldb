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

COMMIT;

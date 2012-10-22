\set ON_ERROR_STOP
BEGIN;

CREATE SCHEMA symboldb;

CREATE TABLE symboldb.package (
  id SERIAL NOT NULL PRIMARY KEY,
  name TEXT NOT NULL,
  epoch INTEGER CHECK (epoch >= 0),
  version TEXT NOT NULL,
  release TEXT NOT NULL,
  arch TEXT NOT NULL,
  hash BYTEA NOT NULL UNIQUE CHECK (LENGTH(hash) = 20)
);
CREATE INDEX ON symboldb.package (name, version);

CREATE TABLE symboldb.file (
  id SERIAL NOT NULL PRIMARY KEY,	 
  package INTEGER NOT NULL
    REFERENCES symboldb.package (id) ON DELETE CASCADE,
  name TEXT NOT NULL,
  user_name TEXT NOT NULL,
  group_name TEXT NOT NULL,
  mtime NUMERIC NOT NULL CHECK (mtime >= 0),
  mode INTEGER NOT NULL CHECK (mode >= 0)
);
CREATE INDEX ON symboldb.file (package);
CREATE INDEX ON symboldb.file (name);

CREATE TABLE symboldb.elf_definition (
  file INTEGER NOT NULL
    REFERENCES symboldb.file (id) ON DELETE CASCADE,
  name TEXT NOT NULL,
  version TEXT,
  primary_version BOOLEAN NOT NULL,
  CHECK (CASE WHEN version IS NULL THEN NOT primary_version ELSE TRUE END)
);
CREATE INDEX ON symboldb.elf_definition (file);
CREATE INDEX ON symboldb.elf_definition (name, version);

CREATE TABLE symboldb.elf_reference (
  file INTEGER NOT NULL
    REFERENCES symboldb.file (id) ON DELETE CASCADE,
  name TEXT NOT NULL,
  version TEXT
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

CREATE TABLE symboldb.elf_error (
  file INTEGER NOT NULL
    REFERENCES symboldb.file (id) ON DELETE CASCADE,
  message TEXT
);

COMMIT;

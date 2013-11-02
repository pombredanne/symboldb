/*
 * Copyright (C) 2013 Red Hat, Inc.
 * Written by Florian Weimer <fweimer@redhat.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <symboldb/rpm_load.hpp>
#include <cxxll/rpm_package_info.hpp>

#include <symboldb/database.hpp>
#include <symboldb/update_elf_closure.hpp>
#include <cxxll/dir_handle.hpp>
#include <cxxll/pg_testdb.hpp>
#include <cxxll/pgconn_handle.hpp>
#include <cxxll/pgresult_handle.hpp>
#include <cxxll/pg_query.hpp>
#include <cxxll/pg_response.hpp>
#include <cxxll/string_support.hpp>
#include <cxxll/read_file.hpp>
#include <cxxll/java_class.hpp>
#include <cxxll/base16.hpp>
#include <cxxll/hash.hpp>
#include <cxxll/checksum.hpp>
#include <cxxll/fd_handle.hpp>
#include <cxxll/fd_source.hpp>
#include <cxxll/read_lines.hpp>
#include <cxxll/source_sink.hpp>
#include <cxxll/temporary_directory.hpp>
#include <symboldb/options.hpp>
#include <symboldb/get_file.hpp>

#include "test.hpp"

using namespace cxxll;

static void
check_rpm_file_list(pgconn_handle &dbh, const char *nvra, const char *filelist_path)
{
  test_section ts(std::string("RPM file list ") + nvra);
  std::vector<std::string> file_list;
  read_lines(filelist_path, file_list);

  pgresult_handle r;
  pg_query(dbh, r, "SELECT COUNT(*) FROM symboldb.package"
	   " WHERE symboldb.nvra(package) = $1", nvra);
  COMPARE_STRING(r.getvalue(0, 0), "1");

  pg_query(dbh, r,
	   "CREATE TEMP TABLE file_list AS"
	   " SELECT symboldb.file_mode_full(fa.mode), fa.caps,"
	   " symboldb.file_flags(fa.flags)::text AS flags,"
	   " file.inode, fa.user_name, fa.group_name,"
	   " file.length, file.name"
	   " FROM (SELECT package_id, attribute_id, name, length, inode"
	   " FROM symboldb.file"
	   " JOIN symboldb.file_contents fc USING (contents_id)"
	   " UNION ALL SELECT package_id, attribute_id, name, 0, 0"
	   " FROM symboldb.directory"
	   " UNION ALL SELECT package_id, attribute_id,"
	   " name || ' -> ' || target, LENGTH(target), 0"
	   " FROM symboldb.symlink) file"
	   " JOIN symboldb.package USING (package_id)"
	   " JOIN symboldb.file_attribute fa USING (attribute_id)"
	   " WHERE symboldb.nvra(package) = $1"
	   " ORDER BY file.name", nvra);
  r.exec(dbh, "COPY file_list TO STDOUT WITH (FORMAT CSV)");
  std::string row;
  for (const std::string &line : file_list) {
    if (dbh.getCopyData(row)) {
      CHECK(!row.empty());
      CHECK(row.at(row.size() - 1) == '\n');
      row.resize(row.size() - 1); // strip '\n' at end
      COMPARE_STRING(row, line);
    } else {
      COMPARE_STRING("", line);
      goto cleanup;
    }
  }
  if (dbh.getCopyData(row)) {
    COMPARE_STRING(row, "");
  }
 cleanup:
  r.exec(dbh, "DROP TABLE file_list");
}

// Reads the test/data/*.filelist files and compares them with the
// matching RPMs.
static void
check_rpm_file_list(pgconn_handle &dbh)
{
  dir_handle dir("test/data");
  while (dirent *ent = dir.readdir()) {
    std::string name(ent->d_name);
    if (ends_with(name, ".filelist")) {
      std::string nvra(name);
      nvra.resize(nvra.size() - strlen(".filelist"));
      name = "test/data/" + name;
      check_rpm_file_list(dbh, nvra.c_str(), name.c_str());
    }
  }
}

static void
check_simple_query(pgconn_handle &dbh, const char *name,
		   const char *path, const char *sql)
{
  test_section ts(name);
  std::vector<std::string> row_vector;
  read_lines(path, row_vector);

  pgresult_handle r;
  r.exec(dbh, (std::string("COPY (") + sql + 
	       ") TO STDOUT WITH (FORMAT CSV)").c_str());
  std::string row;
  for (const std::string &exp : row_vector) {
    if (dbh.getCopyData(row)) {
      CHECK(!row.empty());
      CHECK(row.at(row.size() - 1) == '\n');
      row.resize(row.size() - 1); // strip '\n' at end
      COMPARE_STRING(row, exp);
    } else {
      COMPARE_STRING("", exp);
      return;
    }
  }
  if (dbh.getCopyData(row)) {
    COMPARE_STRING(row, "");
  }
}

// Reads the scripts stored in test/data/rpm-scripts.csv and compares
// them with the contents of the symboldb.package_script table.
static void
check_rpm_scripts(pgconn_handle &dbh)
{
  check_simple_query
    (dbh, "RPM script", "test/data/rpm-scripts.csv",
     "SELECT symboldb.nvra(package), s.kind, s.prog,"
     " replace(s.script, '\n', '^J')"
     " FROM symboldb.package"
     " JOIN symboldb.package_script s USING (package_id)"
     " ORDER BY 1, 2");
}

// Reads the trigger scripts stored in
// test/data/rpm-trigger-scripts.csv and compares them with the
// contents of the symboldb.package_trigger_script table.
static void
check_rpm_trigger_scripts(pgconn_handle &dbh)
{
  check_simple_query
    (dbh, "RPM trigger scripts", "test/data/rpm-trigger-scripts.csv",
     "SELECT symboldb.nvra(package), script_idx, prog,"
     " replace(script, '\n', '^J')"
     " FROM symboldb.package"
     " JOIN symboldb.package_trigger_script USING (package_id)"
     " ORDER BY 1, 2");
}

// Reads the trigger conditions stored in
// test/data/rpm-trigger-conditions.csv and compares them with the
// contents of the symboldb.package_trigger_condition table.
static void
check_rpm_trigger_conditions(pgconn_handle &dbh)
{
  check_simple_query
    (dbh, "RPM trigger scripts", "test/data/rpm-trigger-conditions.csv",
     "SELECT symboldb.nvra(package), c.script_idx,"
     " symboldb.trigger_type(c.flags),"
     " c.name, symboldb.version_comparison(c.flags), c.version"
     " FROM symboldb.package"
     " JOIN symboldb.package_trigger_condition c USING (package_id)"
     " ORDER BY 1, 2, name");
}

// Compares RPM dependencies with test/data/rpm-dependencies.csv.
static void
check_rpm_dependencies(pgconn_handle &dbh)
{
  check_simple_query
    (dbh, "RPM dependencies", "test/data/rpm-dependencies.csv",
     "SELECT symboldb.nevra(package), d.kind::text,"
     " capability, symboldb.version_comparison(d.flags), d.version"
     " FROM symboldb.package"
     " JOIN symboldb.package_dependency d USING (package_id)"
     " ORDER BY 1, 2, 3");
}


static void
test_java_class(database &db, pgconn_handle &conn)
{
  std::vector<unsigned char> buffer;
  read_file("test/data/JavaClass.class", buffer);
  java_class jc(&buffer);
  db.txn_begin_no_sync();
  db.add_java_class(/* fake */ database::contents_id(1), jc);
  db.txn_commit();
  pgresult_handle res;
  res.execBinary
    (conn, "SELECT class_id, name, super_class, access_flags"
     " FROM symboldb.java_class"
     " JOIN symboldb.java_class_contents USING (class_id)"
     " WHERE contents_id = 1");
  CHECK(res.ntuples() == 1);
  int classid, access_flags;
  std::string name, super_class;
  pg_response(res, 0, classid, name, super_class, access_flags);
  COMPARE_STRING(name, "com/redhat/symboldb/test/JavaClass");
  COMPARE_STRING(super_class, "java/lang/Thread");
  CHECK(access_flags == 1 + 16 + 32);
  pg_query_binary
    (conn, res, "SELECT name FROM symboldb.java_interface"
     " WHERE class_id = $1 ORDER BY name", classid);
  CHECK(res.ntuples() == 2);
  COMPARE_STRING(res.getvalue(0, 0), "java/lang/AutoCloseable");
  COMPARE_STRING(res.getvalue(1, 0), "java/lang/Runnable");
  pg_query
    (conn, res, "SELECT name FROM symboldb.java_class_reference"
     " WHERE class_id = $1 ORDER BY name", classid);
  {
    const char *expected[] = {
      "java/lang/AutoCloseable",
      "java/lang/Byte",
      "java/lang/Double",
      "java/lang/Exception",
      "java/lang/Float",
      "java/lang/Integer",
      "java/lang/Long",
      "java/lang/Runnable",
      "java/lang/Short",
      "java/lang/StackOverflowError",
      "java/lang/StringBuilder",
      "java/lang/Thread",
      nullptr
    };
    unsigned end = res.ntuples();
    for (unsigned i = 0; i <= end; ++i) {
      if (i == end) {
	CHECK(expected[i] == nullptr);
      } else if (expected[i] == nullptr) {
	CHECK(false);
	break;
      } else {
	COMPARE_STRING(res.getvalue(i, 0), expected[i]);
      }
    }
  }

  res.exec
    (conn, "SELECT jc.name FROM symboldb.java_class jc"
     " JOIN symboldb.java_class_contents USING (class_id)"
     " JOIN symboldb.file f USING (contents_id)"
     " JOIN symboldb.package p USING (package_id)"
     " WHERE symboldb.nevra(p) = 'objectweb-asm4-0:4.1-2.fc18.noarch'"
     " AND f.name = '/usr/share/java/objectweb-asm4/asm.jar'"
     " ORDER BY jc.name");
  {
    const char *expected[] = {
      "org/objectweb/asm/AnnotationVisitor",
      "org/objectweb/asm/AnnotationWriter",
      "org/objectweb/asm/Attribute",
      "org/objectweb/asm/ByteVector",
      "org/objectweb/asm/ClassReader",
      "org/objectweb/asm/ClassVisitor",
      "org/objectweb/asm/ClassWriter",
      "org/objectweb/asm/Context",
      "org/objectweb/asm/Edge",
      "org/objectweb/asm/FieldVisitor",
      "org/objectweb/asm/FieldWriter",
      "org/objectweb/asm/Frame",
      "org/objectweb/asm/Handle",
      "org/objectweb/asm/Handler",
      "org/objectweb/asm/Item",
      "org/objectweb/asm/Label",
      "org/objectweb/asm/MethodVisitor",
      "org/objectweb/asm/MethodWriter",
      "org/objectweb/asm/Opcodes",
      "org/objectweb/asm/Type",
      "org/objectweb/asm/signature/SignatureReader",
      "org/objectweb/asm/signature/SignatureVisitor",
      "org/objectweb/asm/signature/SignatureWriter",
      nullptr
    };
    unsigned end = res.ntuples();
    CHECK(end > 0);
    for (unsigned i = 0; i <= end; ++i) {
      if (i == end) {
	CHECK(expected[i] == nullptr);
      } else if (expected[i] == nullptr) {
	CHECK(false);
	break;
      } else {
	COMPARE_STRING(res.getvalue(i, 0), expected[i]);
      }
    }
  }

  db.txn_begin();
  db.add_java_error(database::contents_id(1), "error message", "/path");
  db.add_java_error(database::contents_id(2), "error message", "");
  db.txn_rollback();
}

static void
test_xml(database &, pgconn_handle &conn)
{
  pgresult_handle r;
  pg_query
    (conn, r,
     "SELECT xe.message, xe.line, encode(xe.before, 'escape'),"
     " encode(xe.after, 'escape') FROM symboldb.xml_error xe"
     " JOIN symboldb.file USING (contents_id)"
     " WHERE file.name = $1",
     "/usr/share/doc/shared-mime-info-1.1/shared-mime-info-spec.xml" + 0);
  COMPARE_NUMBER(r.ntuples(), 1);
  COMPARE_STRING(r.getvalue(0, 0), "unexpected XML entity");
  COMPARE_STRING(r.getvalue(0, 1), "4");
  COMPARE_STRING(r.getvalue(0, 2),
		 "book/xml/4.1.2/docbookx.dtd\" [\n  <!ENTITY updated ");
  COMPARE_STRING(r.getvalue(0, 3),
		 "\"8 October 2010\">\n  <!ENTITY version \"0.20\">\n]>\n<a");

  r.exec
    (conn,
     "SELECT * FROM symboldb.xml_error JOIN symboldb.file USING (contents_id)"
     " WHERE file.name LIKE '/usr/share/maven-poms/%'");
  COMPARE_NUMBER(r.ntuples(), 0);

  r.exec
    (conn,
     "SELECT file.name, mu.url, mu.type FROM symboldb.java_maven_url mu"
     " JOIN symboldb.file USING (contents_id)"
     " WHERE file.name LIKE '/usr/share/maven-poms/%' ORDER BY 1, 2, 3");
  static const char *file = "/usr/share/maven-poms/JPP.objectweb-asm4-asm-parent.pom";
  struct mu_entry {
    const char *file;
    const char *url;
    const char *type;
  };
  static const mu_entry rows[] = {
    {file, "http://asm.objectweb.org/", "other"},
    {file, "http://asm.objectweb.org/license.html", "other"},
    {file, "http://forge.objectweb.org/tracker/?group_id=23",  "other"},
    {file, "http://svn.forge.objectweb.org/cgi-bin/viewcvs.cgi/asm/trunk/", "scm"},
    {file, "http://www.objectweb.org/", "other"},
    {file, "scm:svn:svn+ssh://${maven.username}@svn.forge.objectweb.org/svnroot/asm/trunk", "developerConnection"},
    {file, "scm:svn:svn://svn.forge.objectweb.org/svnroot/asm/trunk", "connection"},
    {0, 0, 0}
  };
  int row;
  for (row = 0; rows[row].file; ++row) {
    COMPARE_STRING(r.getvalue(row, 0), rows[row].file);
    COMPARE_STRING(r.getvalue(row, 1), rows[row].url);
    COMPARE_STRING(r.getvalue(row, 2), rows[row].type);
  }
  COMPARE_NUMBER(r.ntuples(), row);
}


static void
increment_non_python_cid(pgconn_handle &dbh, database::contents_id &cid)
{
  pgresult_handle res;
  int cid1 = cid.value();
  pg_query_binary
    (dbh, res, "SELECT contents_id FROM symboldb.file_contents"
     " WHERE contents_id > $1 AND contents_id NOT IN"
     " (SELECT contents_id FROM symboldb.python_error"
     " UNION ALL SELECT contents_id FROM symboldb.python_import"
     " UNION ALL SELECT contents_id FROM symboldb.python_attribute"
     " UNION ALL SELECT contents_id FROM symboldb.python_function_def)"
     " ORDER BY contents_id LIMIT 1",
     cid1);
  pg_response(res, 0, cid1);
  cid = database::contents_id(cid1);
}

static void
test()
{
  static const char DBNAME[] = "template1";
  pg_testdb testdb;
  {
    // Run this directly, to suppress notices.
    pgconn_handle db(testdb.connect(DBNAME));
    pgresult_handle res;
    res.exec(db, database::SCHEMA_BASE);
    res.exec(db, database::SCHEMA_INDEX);
  }

  temporary_directory cachedir;
  symboldb_options opt;
  opt.cache_path = cachedir.path();
  opt.output = symboldb_options::quiet;
  database db(testdb.directory().c_str(), DBNAME);

  {
    database::package_id last_pkg_id(0);
    static const char RPMDIR[] = "test/data";
    std::string rpmdir_prefix(RPMDIR);
    rpmdir_prefix += '/';
    dir_handle rpmdir(RPMDIR);
    while (dirent *e = rpmdir.readdir()) {
      if (ends_with(std::string(e->d_name), ".rpm")) {
	std::string rpmpath(rpmdir_prefix + e->d_name);
	checksum csum;
	hash_file(hash_sink::sha256, rpmpath.c_str(), csum);
	csum.type = hash_sink::sha256;
	// Copy the RPM file into the RPM cache.
	{
	  std::unique_ptr<file_cache> fcache(opt.rpm_cache());
	  file_cache::add_sink sink(*fcache, csum);
	  fd_handle fd;
	  fd.open_read_only(rpmpath.c_str());
	  fd_source source(fd.get());
	  copy_source_to_sink(source, sink);
	  std::string p;
	  sink.finish(p);
	}
	rpm_package_info info;
	database::package_id pkg
	  (rpm_load(opt, db, rpmpath.c_str(), info, &csum,
		    ("file://" + rpmpath).c_str()));
	CHECK(pkg > last_pkg_id);
	last_pkg_id = pkg;
	pkg = rpm_load(opt, db, rpmpath.c_str(), info, &csum, nullptr);
	CHECK(pkg == last_pkg_id);
      }
    }
    opt.output = symboldb_options::standard;
  }

  {
    pgconn_handle dbh(testdb.connect(DBNAME));
    {
      pgresult_handle res;
      res.exec(dbh, "SELECT digest, length FROM symboldb.package_digest"
	       " JOIN symboldb.package p USING (package_id)"
	       " WHERE symboldb.nevra(p)"
	       " = 'sysvinit-tools-2.88-9.dsf.fc18.x86_64'"
	       " ORDER BY length(digest)");
      CHECK(res.ntuples() == 2);
      COMPARE_STRING(res.getvalue(0, 0),
		     "\\x44dfb48a42bf902c3e57b844d032642668bcb638");
      COMPARE_STRING(res.getvalue(0, 1), "63824");
      COMPARE_STRING(res.getvalue(1, 0),
		     "\\x800c58cb4760a42c310dbb450455dac8"
		     "3e673598db83d7daefdeb94fce652a62");
      COMPARE_STRING(res.getvalue(1, 1), "63824");

      res.exec(dbh, "SELECT symboldb.nevra(package), symboldb.nvra(package)"
	       " FROM symboldb.package"
	       " WHERE hash = '\\x751e170fd4872df174cd60a5f6379201a295f4e6'");
      CHECK(res.ntuples() == 1);
      COMPARE_STRING(res.getvalue(0, 0), "unzip-6.0-7.fc18.src");
      COMPARE_STRING(res.getvalue(0, 1), "unzip-6.0-7.fc18.src");
    }

    std::vector<database::package_id> pids;
    pgresult_handle r1;
    r1.exec(dbh, "SELECT package_id, name, version, release"
	    " FROM symboldb.package WHERE kind = 'binary'");
    for (int i = 0, endi = r1.ntuples(); i < endi; ++i) {
      {
	int pkg = 0;
	sscanf(r1.getvalue(i, 0), "%d", &pkg);
	CHECK(pkg > 0);
	pids.push_back(database::package_id(pkg));
      }
      if (r1.getvalue(i, 1) == std::string("unzip")) {
	COMPARE_STRING(r1.getvalue(i, 2), "6.0");
	continue;
      } else if (r1.getvalue(i, 1) == std::string("openbios")) {
	COMPARE_STRING(r1.getvalue(i, 2), "1.0.svn1063");
	continue;
      } else if (r1.getvalue(i, 1) == std::string("objectweb-asm4")) {
	COMPARE_STRING(r1.getvalue(i, 2), "4.1");
	continue;
      } else if (r1.getvalue(i, 1) == std::string("xml-writer")) {
	COMPARE_STRING(r1.getvalue(i, 2), "0.2");
	continue;
      } else if (r1.getvalue(i, 1) == std::string("shared-mime-info")) {
	COMPARE_STRING(r1.getvalue(i, 2), "1.1");
	continue;
      } else if (r1.getvalue(i, 1) == std::string("firewalld")) {
	COMPARE_STRING(r1.getvalue(i, 2), "0.2.12");
	continue;
      } else if (r1.getvalue(i, 1) == std::string("kphotobymail")) {
	COMPARE_STRING(r1.getvalue(i, 2), "0.4.1");
	continue;
      } else if (r1.getvalue(i, 1) == std::string("rsh")) {
	COMPARE_STRING(r1.getvalue(i, 2), "0.17");
	continue;
      } else if (r1.getvalue(i, 1) == std::string("cronie")) {
	COMPARE_STRING(r1.getvalue(i, 2), "1.4.10");
	continue;
      } else if (r1.getvalue(i, 1) == std::string("kdenetwork-common")) {
	COMPARE_STRING(r1.getvalue(i, 2), "4.11.2");
	continue;
      }

      COMPARE_STRING(r1.getvalue(i, 1), "sysvinit-tools");
      COMPARE_STRING(r1.getvalue(i, 2), "2.88");

      const char *params[] = {r1.getvalue(i, 0)};
      pgresult_handle r2;
      r2.execParams(dbh, "SELECT name FROM symboldb.directory"
		    " WHERE package_id = $1 ORDER BY name", params);
      CHECK(r2.ntuples() == 1);
      COMPARE_STRING(r2.getvalue(0, 0), "/usr/share/doc/sysvinit-tools-2.88");

      r2.execParams(dbh, "SELECT name, target FROM symboldb.symlink"
		    " WHERE package_id = $1 ORDER BY name", params);
      CHECK(r2.ntuples() == 2);
      COMPARE_STRING(r2.getvalue(0, 0), "/sbin/pidof");
      COMPARE_STRING(r2.getvalue(0, 1), "killall5");
      COMPARE_STRING(r2.getvalue(1, 0), "/usr/bin/lastb");
      COMPARE_STRING(r2.getvalue(1, 1), "last");
    }

    check_rpm_file_list(dbh);
    check_rpm_scripts(dbh);
    check_rpm_trigger_scripts(dbh);
    check_rpm_trigger_conditions(dbh);
    check_rpm_dependencies(dbh);

    // Consistency of file attributes.
    r1.exec(dbh, "SELECT COUNT(*),"
	    " COUNT(DISTINCT (mode, flags, user_name, group_name, caps))"
	    " FROM symboldb.file_attribute");
    COMPARE_STRING(r1.getvalue(0, 0), r1.getvalue(0, 1));

    r1.exec(dbh, "SELECT build_host, build_time FROM symboldb.package"
	    " WHERE symboldb.nevra(package)"
	    " = 'sysvinit-tools-2.88-9.dsf.fc18.x86_64'");
    CHECK(r1.ntuples() == 1);
    COMPARE_STRING(r1.getvalue(0, 0), "x86-05.phx2.fedoraproject.org");
    COMPARE_STRING(r1.getvalue(0, 1), "2012-09-13 15:46:22");

    r1.exec(dbh, "SELECT symboldb.nevra(package) FROM symboldb.package"
	    " WHERE kind = 'source' AND source IS NULL ORDER BY 1");
    COMPARE_NUMBER(r1.ntuples(), 12);
    {
      int row = 0;
      COMPARE_STRING(r1.getvalue(row++, 0), "cronie-1.4.10-7.fc19.src");
      COMPARE_STRING(r1.getvalue(row++, 0), "firewalld-0.2.12-5.fc18.src");
      COMPARE_STRING(r1.getvalue(row++, 0), "kdenetwork-7:4.11.2-1.fc19.src");
      COMPARE_STRING(r1.getvalue(row++, 0), "kphotobymail-0.4.1-11.fc18.src");
      COMPARE_STRING(r1.getvalue(row++, 0), "objectweb-asm4-0:4.1-2.fc18.src");
      COMPARE_STRING(r1.getvalue(row++, 0), "openbios-1.0.svn1063-1.fc18.src");
      COMPARE_STRING(r1.getvalue(row++, 0), "rsh-0.17-72.fc19.src");
      COMPARE_STRING(r1.getvalue(row++, 0), "shared-mime-info-1.1-1.fc18.src");
      COMPARE_STRING(r1.getvalue(row++, 0), "sysvinit-2.88-6.dsf.fc17.src");
      COMPARE_STRING(r1.getvalue(row++, 0), "sysvinit-2.88-9.dsf.fc18.src");
      COMPARE_STRING(r1.getvalue(row++, 0), "unzip-6.0-7.fc18.src");
      COMPARE_STRING(r1.getvalue(row++, 0), "xml-writer-0.2-5.fc18.src");
    }
    r1.exec(dbh, "SELECT COUNT(*) FROM symboldb.package"
	    " JOIN symboldb.file USING (package_id)"
	    " JOIN symboldb.java_class_contents USING (contents_id)"
	    " WHERE kind = 'source'");
    CHECK(r1.ntuples() == 1);
    COMPARE_STRING(r1.getvalue(0, 0), "0");

    r1.exec(dbh,
	    "SELECT DISTINCT"
	    " length, user_name, group_name, mtime, mode,"
	    " encode(digest, 'hex'), encode(contents, 'hex'),"
	    " e_type, soname, encode(build_id, 'hex')"
	    " FROM symboldb.file f"
	    " JOIN symboldb.package p USING (package_id)"
	    " JOIN symboldb.elf_file ef USING (contents_id)"
	    " JOIN symboldb.file_contents fc USING (contents_id)"
	    " JOIN symboldb.file_attribute fa USING (attribute_id)"
	    " WHERE f.name = '/sbin/killall5'"
	    " AND symboldb.nevra(p)"
	    " = 'sysvinit-tools-2.88-9.dsf.fc18.x86_64'");
    CHECK(r1.ntuples() == 1);
    COMPARE_STRING(r1.getvalue(0, 0), "23752");
    COMPARE_STRING(r1.getvalue(0, 1), "root");
    COMPARE_STRING(r1.getvalue(0, 2), "root");
    COMPARE_STRING(r1.getvalue(0, 3), "1347551182");
    COMPARE_STRING(r1.getvalue(0, 4), "33261"); // 0100755
    COMPARE_STRING(r1.getvalue(0, 5),
		   "b75fc6cd2359b0d7d3468be0499ca897"
		   "87234c72fe5b9cf36e4b28cd9a56025c");
    COMPARE_STRING(r1.getvalue(0, 6),
		   "7f454c46020101000000000000000000"
		   "03003e00010000009c1e000000000000"
		   "40000000000000008855000000000000"
		   "0000000040003800090040001d001c00");
    COMPARE_STRING(r1.getvalue(0, 7), "3"); // ET_DYN (sic)
    CHECK(r1.getisnull(0, 8));
    COMPARE_STRING(r1.getvalue(0, 9),
		   "876e6e5ec70f451531bd4ec82a60a1c7f206462a");

    r1.exec(dbh,
	    "SELECT DISTINCT"
	    " length, user_name, group_name, mtime, mode,"
	    " encode(digest, 'hex'), encode(contents, 'hex'),"
	    " e_type, soname, encode(build_id, 'hex')"
	    " FROM symboldb.file f"
	    " JOIN symboldb.package p USING (package_id)"
	    " JOIN symboldb.elf_file ef USING (contents_id)"
	    " JOIN symboldb.file_contents fc USING (contents_id)"
	    " JOIN symboldb.file_attribute fa USING (attribute_id)"
	    " WHERE f.name = '/usr/bin/wall'"
	    " AND symboldb.nevra(p)"
	    " = 'sysvinit-tools-2.88-9.dsf.fc18.x86_64'");
    CHECK(r1.ntuples() == 1);
    COMPARE_STRING(r1.getvalue(0, 0), "15352");
    COMPARE_STRING(r1.getvalue(0, 1), "root");
    COMPARE_STRING(r1.getvalue(0, 2), "tty");
    COMPARE_STRING(r1.getvalue(0, 3), "1347551181");
    COMPARE_STRING(r1.getvalue(0, 4), "34157"); // 0102555
    COMPARE_STRING(r1.getvalue(0, 5),
		   "36fdb67f4d549c4e13790ad836cb5641"
		   "af993ff28a3e623da4f95608653dc55a");
    COMPARE_STRING(r1.getvalue(0, 6),
		   "7f454c46020101000000000000000000"
		   "03003e0001000000cc18000000000000"
		   "4000000000000000b834000000000000"
		   "0000000040003800090040001d001c00");
    COMPARE_STRING(r1.getvalue(0, 7), "3"); // ET_DYN (sic)
    CHECK(r1.getisnull(0, 8));
    COMPARE_STRING(r1.getvalue(0, 9),
		   "36d9f2992247e4afaf292e939c4a3cb25204c142");
    r1.close();

    r1.exec(dbh, "SELECT COUNT(*) FROM symboldb.elf_file WHERE arch IS NULL");
    COMPARE_STRING(r1.getvalue(0, 0), "0");
    r1.exec(dbh, "SELECT DISTINCT p.arch, ef.arch FROM symboldb.package p"
	    " JOIN symboldb.file f USING (package_id)"
	    " JOIN symboldb.elf_file ef USING (contents_id)"
	    " WHERE p.arch::text <> ef.arch::text AND p.name <> 'openbios'");
    CHECK(r1.ntuples() == 1);
    COMPARE_STRING(r1.getvalue(0, 0), "i686");
    COMPARE_STRING(r1.getvalue(0, 1), "i386");

    r1.exec(dbh, "SELECT f.name, f.inode, encode(fc.digest, 'hex'), contents_id"
	    " FROM symboldb.file f JOIN symboldb.package p USING (package_id)"
	    " JOIN symboldb.file_contents fc USING (contents_id)"
	    " WHERE symboldb.nevra(p) = 'unzip-6.0-7.fc18.x86_64'"
	    " AND f.name IN ('/usr/bin/unzip', '/usr/bin/zipinfo')"
	    " ORDER BY f.name");
    CHECK(r1.ntuples() == 2);
    COMPARE_STRING(r1.getvalue(0, 0), "/usr/bin/unzip");
    COMPARE_STRING(r1.getvalue(0, 1), "2");
    COMPARE_STRING(r1.getvalue(0, 2),
		   "8fd9d1fdf0bcee5715e347313f5e43a9"
		   "207f3404c03f4e0fe5c1108e0d0f6c4d");
    COMPARE_STRING(r1.getvalue(1, 0), "/usr/bin/zipinfo");
    COMPARE_STRING(r1.getvalue(1, 1), "2");
    COMPARE_STRING(r1.getvalue(0, 2),
		   "8fd9d1fdf0bcee5715e347313f5e43a9"
		   "207f3404c03f4e0fe5c1108e0d0f6c4d");
    COMPARE_STRING(r1.getvalue(0, 3), r1.getvalue(1, 3));
    r1.exec(dbh, "SELECT COUNT(*) FROM symboldb.file f"
	    " JOIN symboldb.package p USING (package_id)"
	    " JOIN symboldb.file_contents fc USING (contents_id)"
	    " WHERE symboldb.nevra(p) = 'unzip-6.0-7.fc18.x86_64'"
	    " AND inode = 2"
	    " AND f.name NOT IN ('/usr/bin/unzip', '/usr/bin/zipinfo')");
    COMPARE_STRING(r1.getvalue(0, 0), "0");

    r1.exec(dbh,
	    "SELECT name || ':' || arch FROM symboldb.file"
	    " JOIN symboldb.elf_file USING (contents_id)"
	    " WHERE name LIKE '/usr/share/qemu/openbios-%' ORDER BY name");
    CHECK(r1.ntuples() == 3);
    COMPARE_STRING(r1.getvalue(0, 0), "/usr/share/qemu/openbios-ppc:ppc");
    COMPARE_STRING(r1.getvalue(1, 0), "/usr/share/qemu/openbios-sparc32:sparc");
    COMPARE_STRING(r1.getvalue(2, 0),
		   "/usr/share/qemu/openbios-sparc64:sparc64");

    r1.exec
      (dbh, "SELECT type, file_offset, virt_addr, phys_addr, file_size,"
       " memory_size, align, readable, writable, executable"
       " FROM symboldb.file_contents"
       " JOIN symboldb.elf_program_header USING (contents_id)"
       " WHERE digest = '\\x4df4ba37989b27afe6b7aeb45f6b854321af968ac23004027e9fcd6bda20a775'"
       " ORDER BY file_offset, type");
    COMPARE_NUMBER(r1.ntuples(), 8);
    {
      long long type, file_offset, virt_addr, phys_addr, file_size, memory_size;
      int align;
      bool readable, writable, executable;

      pg_response(r1, 0, type, file_offset, virt_addr, phys_addr, file_size,
		  memory_size, align, readable, writable, executable);
      COMPARE_NUMBER(type, 1LL); // PT_LOAD
      COMPARE_NUMBER(file_offset, 0LL);
      COMPARE_NUMBER(virt_addr, 0x0000000000400000LL);
      COMPARE_NUMBER(phys_addr, 0x0000000000400000LL);
      COMPARE_NUMBER(file_size, 0x00217cLL);
      COMPARE_NUMBER(memory_size, 0x00217cLL);
      COMPARE_NUMBER(align, 0x200000);
      CHECK(readable);
      CHECK(!writable);
      CHECK(executable);

      pg_response(r1, 1, type, file_offset, virt_addr, phys_addr, file_size,
		  memory_size, align, readable, writable, executable);
      COMPARE_NUMBER(type, 0x6474e551LL); // PT_GNU_STACK
      COMPARE_NUMBER(file_offset, 0LL);
      COMPARE_NUMBER(virt_addr, 0LL);
      COMPARE_NUMBER(phys_addr, 0LL);
      COMPARE_NUMBER(file_size, 0LL);
      COMPARE_NUMBER(memory_size, 0LL);
      COMPARE_NUMBER(align, 8);
      CHECK(readable);
      CHECK(writable);
      CHECK(!executable);

      pg_response(r1, 2, type, file_offset, virt_addr, phys_addr, file_size,
		  memory_size, align, readable, writable, executable);
      COMPARE_NUMBER(type, 6LL); // PT_PHDR
      COMPARE_NUMBER(file_offset, 0x40LL);
      COMPARE_NUMBER(virt_addr, 0x0000000000400040LL);
      COMPARE_NUMBER(phys_addr, 0x0000000000400040LL);
      COMPARE_NUMBER(file_size, 0x1c0LL);
      COMPARE_NUMBER(memory_size, 0x1c0LL);
      COMPARE_NUMBER(align, 8);
      CHECK(readable);
      CHECK(!writable);
      CHECK(executable);

      pg_response(r1, 6, type, file_offset, virt_addr, phys_addr, file_size,
		  memory_size, align, readable, writable, executable);
      COMPARE_NUMBER(type, 1LL); // PT_LOAD
      COMPARE_NUMBER(file_offset, 0x2180LL);
      COMPARE_NUMBER(virt_addr, 0x602180LL);
      COMPARE_NUMBER(phys_addr, 0x602180LL);
      COMPARE_NUMBER(file_size, 0x3b8LL);
      COMPARE_NUMBER(memory_size, 0x508LL);
      COMPARE_NUMBER(align, 0x200000);
      CHECK(readable);
      CHECK(writable);
      CHECK(!executable);
    }

    r1.exec
      (dbh, "SELECT interp"
       " FROM symboldb.file_contents"
       " JOIN symboldb.elf_file USING (contents_id)"
       " WHERE digest = '\\x4df4ba37989b27afe6b7aeb45f6b854321af968ac23004027e9fcd6bda20a775'");
    COMPARE_NUMBER(r1.ntuples(), 1);
    COMPARE_STRING(r1.getvalue(0, 0), "/lib64/ld-linux-x86-64.so.2");

    r1.exec
      (dbh, "SELECT tag, value"
       " FROM symboldb.file_contents"
       " JOIN symboldb.elf_dynamic USING (contents_id)"
       " WHERE digest = '\\x4df4ba37989b27afe6b7aeb45f6b854321af968ac23004027e9fcd6bda20a775'"
       " ORDER BY 1, 2");
    {
      struct entry {
	long long tag;
	long long value;
      };
      static const entry entries[] = {
	{2, 1152},		    // PLTRELSZ
	{3, 0x0000000000602390LL},  // PLTGOT
	{5, 0x00000000004007d8LL},  // STRTAB
	{6, 0x0000000000400298LL},  // SYMTAB
	{7, 0x0000000000400b58LL},  // RELA
	{8, 96},		    // RELASZ
	{9, 24},		    // RELAENT
	{10, 649},		    // STRSZ
	{11, 24},		    // SYMENT
	{12, 0x0000000000401038LL}, // INIT
	{13, 0x0000000000401e74LL}, // FINI
	{20, 7},		    // PLTREL
	{21, 0},		    // DEBUG
	{23, 0x0000000000400bb8LL}, // JMPREL
	{25, 0x0000000000602180LL}, // INIT_ARRAY
	{26, 0x0000000000602188LL}, // FINI_ARRAY
	{27, 8},		    // INIT_ARRAYSZ
	{28, 8},		    // FINI_ARRAYSZ
	{0x6ffffef5LL, 0x400260LL}, // GNU_HASH
	{0x6ffffff0LL, 0x400a62LL}, // VERSYM
	{0x6ffffffeLL, 0x400ad8LL}, // VERNEED
	{0x6fffffffLL, 2},	    // VERNEEDNUM
	{-1, -1}		    // sentinel
      };
      int row = 0;
      const struct entry *expected;
      for (expected = entries; expected->tag > 0; ++expected) {
	entry actual;
	pg_response(r1, row++, actual.tag, actual.value);
	COMPARE_NUMBER(actual.tag, expected->tag);
	COMPARE_NUMBER(actual.value, expected->value);
      }
      COMPARE_NUMBER(r1.ntuples(), expected - entries);
    }
 
    r1.exec(dbh,
	    "SELECT symboldb.nevra(package), length, flags FROM symboldb.file"
	    " JOIN symboldb.file_contents USING (contents_id)"
	    " JOIN symboldb.package USING (package_id)"
	    " JOIN symboldb.file_attribute fa USING (attribute_id)"
	    " WHERE file.name = '/usr/share/mime/types'");
    CHECK(r1.ntuples() == 1);
    COMPARE_STRING(r1.getvalue(0, 0), "shared-mime-info-1.1-1.fc18.x86_64");
    COMPARE_STRING(r1.getvalue(0, 1), "0");
    COMPARE_STRING(r1.getvalue(0, 2), "64"); // ghost file

    CHECK(!pids.empty());
    db.txn_begin();
    database::package_set_id pset(db.create_package_set("test-set"));
    CHECK(pset.value() > 0);
    CHECK(!db.update_package_set(pset, pids.begin(), pids.begin()));
    CHECK(db.update_package_set(pset, pids));
    CHECK(!db.update_package_set(pset, pids));
    db.txn_commit();

    r1.exec(dbh, "SELECT * FROM symboldb.package_set_member");
    CHECK(r1.ntuples() == static_cast<int>(pids.size()));

    db.txn_begin();
    CHECK(db.update_package_set(pset, pids.begin() + 1, pids.end()));
    CHECK(!db.update_package_set(pset, pids.begin() + 1, pids.end()));
    db.txn_commit();
    r1.exec(dbh, "SELECT * FROM symboldb.package_set_member");
    CHECK(r1.ntuples() == static_cast<int>(pids.size() - 1));
    char pkgstr[32];
    snprintf(pkgstr, sizeof(pkgstr), "%d", pids.front().value());
    {
      const char *params[] = {pkgstr};
      r1.execParams(dbh,
		    "SELECT * FROM symboldb.package_set_member"
		    " WHERE package_id = $1", params);
      CHECK(r1.ntuples() == 0);
    }
    db.txn_begin();
    CHECK(db.update_package_set(pset, pids.begin(), pids.begin() + 1));
    CHECK(!db.update_package_set(pset, pids.begin(), pids.begin() + 1));
    db.txn_commit();
    r1.exec(dbh, "SELECT package_id FROM symboldb.package_set_member");
    CHECK(r1.ntuples() == 1);
    COMPARE_STRING(r1.getvalue(0, 0), pkgstr);

    testdb.exec_test_sql(DBNAME, "DELETE FROM symboldb.package_set_member");
    char psetstr[32];
    snprintf(psetstr, sizeof(psetstr), "%d", pset.value());
    {
      const char *params[] = {psetstr};
      r1.execParams(dbh,
		    "INSERT INTO symboldb.package_set_member"
		    " SELECT $1, package_id FROM symboldb.package"
		    " WHERE arch IN ('x86_64', 'i686')", params);
    }
    r1.exec(dbh, "BEGIN");
    update_elf_closure(dbh, pset, nullptr);
    r1.exec(dbh, "COMMIT");

    std::vector<std::vector<unsigned char> > digests;
    db.referenced_package_digests(digests);
    COMPARE_NUMBER(digests.size(), 28U); // 16 packages with 2 digests each

    {
      std::vector<unsigned char> digest;
      digest.resize(32);
      CHECK(db.package_by_digest(digest).value() == 0);
    }

    // Test get_file().
    {
      std::vector<unsigned char> digest;
      base16_decode("8fd9d1fdf0bcee5715e347313f5e43a9"
		    "207f3404c03f4e0fe5c1108e0d0f6c4d",
		    std::back_inserter(digest));
      {
	hash_sink hash(hash_sink::sha256);
	CHECK(get_file(opt, db, digest, hash));
	std::vector<unsigned char> result;
	hash.digest(result);
	CHECK(result == digest);
      }

      digest.clear();
      base16_decode("b75fc6cd2359b0d7d3468be0499ca897"
		   "87234c72fe5b9cf36e4b28cd9a56025c",
		    std::back_inserter(digest));
      {
	hash_sink hash(hash_sink::sha256);
	CHECK(get_file(opt, db, digest, hash));
	std::vector<unsigned char> result;
	hash.digest(result);
	CHECK(result == digest);
      }
    }

    // Test contents preservation for files in /etc.
    {
      r1.exec(dbh, "SELECT LENGTH(contents) FROM symboldb.file_contents"
	      " WHERE digest = "
	      "'\\xf96d066e6deb9c21b0e5d42b704e9b32"
	      "22c365185e83593133c0115db69e16c8'");
      COMPARE_NUMBER(r1.ntuples(), 1);
      COMPARE_STRING(r1.getvalue(0, 0), "72");
    }

    // Test SQL syntax for Python-related loaders.
    {
      db.txn_begin();
      database::contents_id cid;

      increment_non_python_cid(dbh, cid);
      CHECK(!db.has_python_analysis(cid));
      db.add_python_import(cid, "abc");
      CHECK(db.has_python_analysis(cid));

      increment_non_python_cid(dbh, cid);
      CHECK(!db.has_python_analysis(cid));
      db.add_python_error(cid, 17, "syntax error");
      db.add_python_error(cid, 0, "other error");
      CHECK(!db.has_python_analysis(cid));

      increment_non_python_cid(dbh, cid);
      CHECK(!db.has_python_analysis(cid));
      db.add_python_attribute(cid, "attr");
      CHECK(db.has_python_analysis(cid));

      increment_non_python_cid(dbh, cid);
      CHECK(!db.has_python_analysis(cid));
      db.add_python_function_def(cid, "func");
      CHECK(db.has_python_analysis(cid));

      increment_non_python_cid(dbh, cid);
      CHECK(!db.has_python_analysis(cid));
      db.add_python_class_def(cid, "Class");
      CHECK(db.has_python_analysis(cid));

      db.txn_rollback();
    }

    // Test Python import extraction.
    {
      r1.exec(dbh, "SELECT python_import.name FROM symboldb.python_import"
	      " JOIN symboldb.file USING (contents_id)"
	      " WHERE file.name = '/usr/sbin/firewalld'"
	      " ORDER BY 1");
      int row = 0;
      COMPARE_STRING(r1.getvalue(row++, 0), "dbus");
      COMPARE_STRING(r1.getvalue(row++, 0), "firewall.config");
      COMPARE_STRING(r1.getvalue(row++, 0), "firewall.core.logger.FileLog");
      COMPARE_STRING(r1.getvalue(row++, 0), "firewall.core.logger.log");
      COMPARE_STRING(r1.getvalue(row++, 0), "firewall.errors.*");
      COMPARE_STRING(r1.getvalue(row++, 0),
		     "firewall.functions.firewalld_is_active");
      COMPARE_STRING(r1.getvalue(row++, 0), "firewall.server.server");
      COMPARE_STRING(r1.getvalue(row++, 0), "os");
      COMPARE_STRING(r1.getvalue(row++, 0), "sys");
      COMPARE_STRING(r1.getvalue(row++, 0), "syslog");
      COMPARE_STRING(r1.getvalue(row++, 0), "traceback");
      CHECK(row == r1.ntuples());

      {
	static const char *const attributes[] = {
	  "DBusException",
	  "DEBUG_MAX",
	  "ERROR",
	  "FATAL",
	  "FIREWALLD_LOGFILE",
	  "INFO_MAX",
	  "NO_DEBUG",
	  "NO_INFO",
	  "WARNING",
	  "addDebugLogging",
	  "addInfoLogging",
	  "argv",
	  "chdir",
	  "close",
	  "errno",
	  "error",
	  "exit",
	  "fatal",
	  "fork",
	  "format_exc",
	  "getpid",
	  "getuid",
	  "remove",
	  "run_server",
	  "setDateFormat",
	  "setDebugLogLevel",
	  "setDebugLogging",
	  "setFormat",
	  "setInfoLogLevel",
	  "setInfoLogging",
	  "setsid",
	  "stdout",
	  "strerror",
	  "syslog",
	  "umask",
	  "write",
	  nullptr
	};

	r1.exec(dbh, "SELECT python_attribute.name"
		" FROM symboldb.python_attribute"
		" JOIN symboldb.file USING (contents_id)"
		" WHERE file.name = '/usr/sbin/firewalld'"
		" ORDER BY 1");
	for (row = 0; attributes[row]; ++row) {
	  COMPARE_STRING(r1.getvalue(row, 0), attributes[row]);
	}
	COMPARE_NUMBER(row, r1.ntuples());
      }

      r1.exec(dbh, "SELECT python_import.name FROM symboldb.python_import"
	      " JOIN symboldb.file USING (contents_id)"
	      " WHERE file.name = '/usr/lib/python2.7/site-packages"
	      "/firewall/core/io/service.py'"
	      " ORDER BY 1");
      row = 0;
      COMPARE_STRING(r1.getvalue(row++, 0), "firewall.config._");
      COMPARE_STRING(r1.getvalue(row++, 0), "firewall.core.io.io_object.*");
      COMPARE_STRING(r1.getvalue(row++, 0), "firewall.core.logger.log");
      COMPARE_STRING(r1.getvalue(row++, 0), "firewall.errors.*");
      COMPARE_STRING(r1.getvalue(row++, 0), "firewall.functions");
      COMPARE_STRING(r1.getvalue(row++, 0), "os");
      COMPARE_STRING(r1.getvalue(row++, 0), "shutil");
      COMPARE_STRING(r1.getvalue(row++, 0), "xml.sax");
      CHECK(row == r1.ntuples());

      {
	static const char *const functions[] = {
	  "__init__",
	  "_check_config",
	  "service_reader",
	  "service_writer",
	  "startElement",
	  nullptr
	};

	r1.exec(dbh, "SELECT python_function_def.name"
		" FROM symboldb.python_function_def"
		" JOIN symboldb.file USING (contents_id)"
		" WHERE file.name = '/usr/lib/python2.7/site-packages"
		"/firewall/core/io/service.py'"
		" ORDER BY 1");
	for (row = 0; functions[row]; ++row) {
	  COMPARE_STRING(r1.getvalue(row, 0), functions[row]);
	}
	COMPARE_NUMBER(row, r1.ntuples());
      }

      {
	static const char *const classes[] = {
	  "Service",
	  "service_ContentHandler",
	  nullptr
	};

	r1.exec(dbh, "SELECT python_class_def.name"
		" FROM symboldb.python_class_def"
		" JOIN symboldb.file USING (contents_id)"
		" WHERE file.name = '/usr/lib/python2.7/site-packages"
		"/firewall/core/io/service.py'"
		" ORDER BY 1");
	for (row = 0; classes[row]; ++row) {
	  COMPARE_STRING(r1.getvalue(row, 0), classes[row]);
	}
	COMPARE_NUMBER(row, r1.ntuples());
      }
    }

    db.txn_begin();
    db.expire_url_cache();
    db.expire_packages();
    db.expire_file_contents();
    db.expire_java_classes();
    db.txn_rollback();

    test_java_class(db, dbh);
    test_xml(db, dbh);
  }

  // FIXME: Add more sanity check on database contents.
}

static test_register t("rpm_load", test);

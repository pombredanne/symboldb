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
#include <symboldb/options.hpp>

#include "test.hpp"

using namespace cxxll;

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
      NULL
    };
    unsigned end = res.ntuples();
    for (unsigned i = 0; i <= end; ++i) {
      if (i == end) {
	CHECK(expected[i] == NULL);
      } else if (expected[i] == NULL) {
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
      NULL
    };
    unsigned end = res.ntuples();
    CHECK(end > 0);
    for (unsigned i = 0; i <= end; ++i) {
      if (i == end) {
	CHECK(expected[i] == NULL);
      } else if (expected[i] == NULL) {
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
test()
{
  static const char DBNAME[] = "template1";
  pg_testdb testdb;
  {
    // Run this directly, to suppress notices.
    pgconn_handle db(testdb.connect(DBNAME));
    pgresult_handle res;
    res.exec(db, database::SCHEMA);
  }

  symboldb_options opt;
  opt.output = symboldb_options::quiet;
  database db(testdb.directory().c_str(), DBNAME);

  {
    database::package_id last_pkg_id(0);
    static const char RPMDIR[] = "test/data";
    std::string rpmdir_prefix(RPMDIR);
    rpmdir_prefix += '/';
    dir_handle rpmdir(RPMDIR);
    while (dirent *e = rpmdir.readdir()) {
      if (ends_with(std::string(e->d_name), ".rpm")
	  && !ends_with(std::string(e->d_name), ".src.rpm")) {
	rpm_package_info info;
	database::package_id pkg
	  (rpm_load(opt, db, (rpmdir_prefix + e->d_name).c_str(), info, NULL));
	CHECK(pkg > last_pkg_id);
	last_pkg_id = pkg;
	pkg = rpm_load(opt, db, (rpmdir_prefix + e->d_name).c_str(), info,
		       NULL);
	CHECK(pkg == last_pkg_id);
      }
    }
  }

  {
    static const char *const sysvinit_files_6[] = {
      "/sbin/killall5",
      "/sbin/sulogin",
      "/usr/bin/last",
      "/usr/bin/mesg",
      "/usr/bin/utmpdump",
      "/usr/bin/wall",
      "/usr/share/doc/sysvinit-tools-2.88/COPYRIGHT",
      "/usr/share/doc/sysvinit-tools-2.88/Changelog",
      "/usr/share/man/man1/last.1.gz",
      "/usr/share/man/man1/lastb.1.gz",
      "/usr/share/man/man1/mesg.1.gz",
      "/usr/share/man/man1/utmpdump.1.gz",
      "/usr/share/man/man1/wall.1.gz",
      "/usr/share/man/man8/killall5.8.gz",
      "/usr/share/man/man8/pidof.8.gz",
      "/usr/share/man/man8/sulogin.8.gz",
      NULL
    };
    static const char *const sysvinit_files_9[] = {
      "/sbin/killall5",
      "/usr/bin/last",
      "/usr/bin/mesg",
      "/usr/bin/wall",
      "/usr/share/doc/sysvinit-tools-2.88/COPYRIGHT",
      "/usr/share/doc/sysvinit-tools-2.88/Changelog",
      "/usr/share/man/man1/last.1.gz",
      "/usr/share/man/man1/lastb.1.gz",
      "/usr/share/man/man1/mesg.1.gz",
      "/usr/share/man/man1/wall.1.gz",
      "/usr/share/man/man8/killall5.8.gz",
      "/usr/share/man/man8/pidof.8.gz",
      NULL
    };

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
    }

    std::vector<database::package_id> pids;
    pgresult_handle r1;
    r1.exec(dbh, "SELECT package_id, name, version, release"
	    " FROM symboldb.package");
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
      }

      COMPARE_STRING(r1.getvalue(i, 1), "sysvinit-tools");
      COMPARE_STRING(r1.getvalue(i, 2), "2.88");
      std::string release(r1.getvalue(i, 3));
      const char *const *files;
      if (starts_with(release, "6")) {
	files = sysvinit_files_6;
      } else if (starts_with(release, "9")) {
	files = sysvinit_files_9;
      } else {
	CHECK(false);
	break;
      }
      const char *params[] = {r1.getvalue(i, 0)};
      pgresult_handle r2;
      r2.execParams(dbh,
		    "SELECT name FROM symboldb.file"
		    " WHERE package_id = $1 ORDER BY 1", params);
      int endj = r2.ntuples();
      CHECK(endj > 0);
      for (int j = 0; j < endj; ++j) {
	CHECK(files[j] != NULL);
	if (files[j] == NULL) {
	  break;
	}
	COMPARE_STRING(r2.getvalue(j, 0), files[j]);
      }
      CHECK(files[endj] == NULL);

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

    r1.exec(dbh, "SELECT build_host, build_time FROM symboldb.package"
	    " WHERE symboldb.nevra(package)"
	    " = 'sysvinit-tools-2.88-9.dsf.fc18.x86_64'");
    CHECK(r1.ntuples() == 1);
    COMPARE_STRING(r1.getvalue(0, 0), "x86-05.phx2.fedoraproject.org");
    COMPARE_STRING(r1.getvalue(0, 1), "2012-09-13 15:46:22");

    r1.exec(dbh,
	    "SELECT DISTINCT"
	    " length, user_name, group_name, mtime, mode,"
	    " encode(digest, 'hex'), encode(contents, 'hex'),"
	    " e_type, soname, encode(build_id, 'hex')"
	    " FROM symboldb.file f"
	    " JOIN symboldb.package p USING (package_id)"
	    " JOIN symboldb.elf_file ef USING (contents_id)"
	    " JOIN symboldb.file_contents fc USING (contents_id)"
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
    update_elf_closure(dbh, pset, NULL);
    r1.exec(dbh, "COMMIT");

    std::vector<std::vector<unsigned char> > digests;
    db.referenced_package_digests(digests);
    CHECK(digests.size() == 10); // 5 packages with 2 digests each

    {
      std::vector<unsigned char> digest;
      digest.resize(32);
      CHECK(db.package_by_digest(digest).value() == 0);
    }

    db.txn_begin();
    db.expire_packages();
    db.expire_file_contents();
    db.expire_java_classes();
    db.txn_rollback();

    test_java_class(db, dbh);
  }

  // FIXME: Add more sanity check on database contents.
}

static test_register t("rpm_load", test);

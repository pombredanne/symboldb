cmake_minimum_required (VERSION 2.6)
project (symboldb)

include(CheckCSourceCompiles)

if(CMAKE_COMPILER_IS_GNUCXX)
  set(CMAKE_CXX_FLAGS "-std=gnu++03 -O2 -g -Wall -W -fstack-protector -D_GNU_SOURCE -D_FORTIFY_SOURCE=2")
  set(CMAKE_EXE_LINKER_FLAGS "-g")
endif()

find_path (
  NSS_INCLUDE_DIR pk11pub.h
  PATH_SUFFIXES include/nss include/nss3
)

find_path (
  NSPR_INCLUDE_DIR prtypes.h
  PATH_SUFFIXES include/nspr include/nspr4
)

find_path (
  PQ_INCLUDE_DIR libpq-fe.h
  PATH_SUFFIXES include include/postgresql
)

include_directories (
  ${PROJECT_SOURCE_DIR}/include
  ${NSS_INCLUDE_DIR}
  ${NSPR_INCLUDE_DIR}
  ${PQ_INCLUDE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
)

CHECK_C_SOURCE_COMPILES ("#include <libpq-fe.h>
int main() { PGRES_SINGLE_TUPLE; return 0; }
"
  HAVE_PG_SINGLE_TUPLE
)

configure_file (
  "${PROJECT_SOURCE_DIR}/symboldb_config.h.in"
  "${PROJECT_BINARY_DIR}/symboldb_config.h"
)

add_custom_command (
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/error-constants.inc
  COMMAND bash ${PROJECT_SOURCE_DIR}/error-constants.sh ${CMAKE_C_COMPILER} > ${CMAKE_CURRENT_BINARY_DIR}/error-constants.inc
  DEPENDS error-constants.sh
)

add_custom_command (
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/schema-base.sql.inc
  COMMAND xxd -i < ${PROJECT_SOURCE_DIR}/schema-base.sql > ${CMAKE_CURRENT_BINARY_DIR}/schema-base.sql.inc
  DEPENDS schema-base.sql
)

add_custom_command (
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/schema-index.sql.inc
  COMMAND xxd -i < ${PROJECT_SOURCE_DIR}/schema-index.sql > ${CMAKE_CURRENT_BINARY_DIR}/schema-index.sql.inc
  DEPENDS schema-index.sql
)

add_custom_command (
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/python_imports.py.inc
  COMMAND xxd -i < ${PROJECT_SOURCE_DIR}/lib/cxxll/python_imports.py > ${CMAKE_CURRENT_BINARY_DIR}/python_imports.py.inc
  DEPENDS lib/cxxll/python_imports.py
)

add_library (CXXLL
  lib/cxxll/checksum.cpp
  lib/cxxll/base16.cpp
  lib/cxxll/cond.cpp
  lib/cxxll/cpio_reader.cpp
  lib/cxxll/curl_exception.cpp
  lib/cxxll/curl_exception_dump.cpp
  lib/cxxll/curl_fetch_result.cpp
  lib/cxxll/curl_handle.cpp
  lib/cxxll/dir_handle.cpp
  lib/cxxll/elf_exception.cpp
  lib/cxxll/elf_image.cpp
  lib/cxxll/elf_symbol.cpp
  lib/cxxll/elf_symbol_definition.cpp
  lib/cxxll/elf_symbol_reference.cpp
  lib/cxxll/eof_exception.cpp
  lib/cxxll/expat_handle.cpp
  lib/cxxll/expat_minidom.cpp
  lib/cxxll/expat_source.cpp
  lib/cxxll/fd_handle.cpp
  lib/cxxll/fd_sink.cpp
  lib/cxxll/fd_source.cpp
  lib/cxxll/file_cache.cpp
  lib/cxxll/file_handle.cpp
  lib/cxxll/gunzip_source.cpp
  lib/cxxll/hash.cpp
  lib/cxxll/java_class.cpp
  lib/cxxll/memory_range_source.cpp
  lib/cxxll/mutex.cpp
  lib/cxxll/os.cpp
  lib/cxxll/os_error_string.cpp
  lib/cxxll/os_exception.cpp
  lib/cxxll/os_exception_function.cpp
  lib/cxxll/os_exception_defaults.cpp
  lib/cxxll/os_current_directory.cpp
  lib/cxxll/os_readlink.cpp
  lib/cxxll/os_remove_directory_tree.cpp
  lib/cxxll/os_ticks.cpp
  lib/cxxll/pg_exception.cpp
  lib/cxxll/pg_private.cpp
  lib/cxxll/pg_testdb.cpp
  lib/cxxll/pgconn_handle.cpp
  lib/cxxll/pgresult_handle.cpp
  lib/cxxll/python_imports.cpp
  lib/cxxll/read_file.cpp
  lib/cxxll/regex_handle.cpp
  lib/cxxll/rpm_evr.cpp
  lib/cxxll/rpm_dependency.cpp
  lib/cxxll/rpm_file_entry.cpp
  lib/cxxll/rpm_file_info.cpp
  lib/cxxll/rpm_package_info.cpp
  lib/cxxll/rpm_parser.cpp
  lib/cxxll/rpm_parser_exception.cpp
  lib/cxxll/rpmfi_handle.cpp
  lib/cxxll/rpmtd_wrapper.cpp
  lib/cxxll/sink.cpp
  lib/cxxll/source.cpp
  lib/cxxll/source_sink.cpp
  lib/cxxll/string_sink.cpp
  lib/cxxll/string_source.cpp
  lib/cxxll/string_support.cpp
  lib/cxxll/subprocess.cpp
  lib/cxxll/task.cpp
  lib/cxxll/tee_sink.cpp
  lib/cxxll/temporary_directory.cpp
  lib/cxxll/url.cpp
  lib/cxxll/utf8.cpp
  lib/cxxll/vector_extract.cpp
  lib/cxxll/vector_sink.cpp
  lib/cxxll/zip_file.cpp
  lib/cxxll/zlib.cpp
  lib/cxxll/zlib_inflate_exception.cpp
  ${CMAKE_CURRENT_BINARY_DIR}/error-constants.inc
  ${CMAKE_CURRENT_BINARY_DIR}/python_imports.py.inc
)

add_library (SymbolDB
  lib/symboldb/database.cpp
  lib/symboldb/download.cpp
  lib/symboldb/download_repo.cpp
  lib/symboldb/expire.cpp
  lib/symboldb/get_file.cpp
  lib/symboldb/options.cpp
  lib/symboldb/repomd.cpp
  lib/symboldb/repomd_primary.cpp
  lib/symboldb/repomd_primary_xml.cpp
  lib/symboldb/rpm_load.cpp
  lib/symboldb/show_source_packages.cpp
  lib/symboldb/update_elf_closure.cpp
  ${CMAKE_CURRENT_BINARY_DIR}/schema-base.sql.inc
  ${CMAKE_CURRENT_BINARY_DIR}/schema-index.sql.inc
)

target_link_libraries (CXXLL
  -larchive
  -lcurl
  -ldl
  -lelf
  -lexpat
  -lnss3
  -lpq
  -lrpm -lrpmio
  -lrt
  -lz
)

target_link_libraries (SymbolDB
  CXXLL
)

target_link_libraries (SymbolDB
  -lcurl
  -ldl
  -lelf
  -lexpat
  -lnss3
  -lpq
  -lrpm -lrpmio
  -lz
)

add_executable (symboldb
  src/symboldb.cpp
)

target_link_libraries (symboldb
  SymbolDB
)

install (TARGETS symboldb DESTINATION bin)

add_executable (pgtestshell
  src/pgtestshell.cpp
)

target_link_libraries (pgtestshell
  CXXLL
)

install (TARGETS pgtestshell DESTINATION bin)

add_executable (runtests
  test/runtests.cpp
  test/test-base16.cpp
  test/test-bounded_ordered_queue.cpp
  test/test-dir_handle.cpp
  test/test-download.cpp
  test/test-fd_handle.cpp
  test/test-fd_sink.cpp
  test/test-fd_source.cpp
  test/test-file_cache.cpp
  test/test-endian.cpp
  test/test-expat_minidom.cpp
  test/test-expat_source.cpp
  test/test-gunzip_source.cpp
  test/test-java_class.cpp
  test/test-os.cpp
  test/test-os_exception.cpp
  test/test-pg_testdb.cpp
  test/test-python_imports.cpp
  test/test-read_file.cpp
  test/test-regex_handle.cpp
  test/test-repomd.cpp
  test/test-rpm_load.cpp
  test/test-string_source.cpp
  test/test-string_support.cpp
  test/test-subprocess.cpp
  test/test-task.cpp
  test/test-temporary_directory.cpp
  test/test-utf8.cpp
  test/test-vector_extract.cpp
  test/test-zip_file.cpp
  test/test.cpp
)

target_link_libraries (runtests
  SymbolDB
)

add_custom_command (
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/symboldb.1
  COMMAND xmlto man -o ${CMAKE_CURRENT_BINARY_DIR} ${PROJECT_SOURCE_DIR}/doc/symboldb.xml
  DEPENDS ${PROJECT_SOURCE_DIR}/doc/symboldb.xml
)

add_custom_target(symboldb.manpage ALL DEPENDS symboldb.1)

install (
  FILES ${CMAKE_CURRENT_BINARY_DIR}/symboldb.1
  DESTINATION share/man/man1
)

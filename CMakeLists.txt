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
  ${PROJECT_SOURCE_DIR}
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
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/schema.sql.inc
  COMMAND xxd -i < ${PROJECT_SOURCE_DIR}/schema.sql > ${CMAKE_CURRENT_BINARY_DIR}/schema.sql.inc
  DEPENDS schema.sql
)

add_library (CXXLL
  checksum.cpp
  base16.cpp
  cpio_reader.cpp
  curl_exception.cpp
  curl_exception_dump.cpp
  curl_fetch_result.cpp
  curl_handle.cpp
  dir_handle.cpp
  download.cpp
  elf_exception.cpp
  elf_image.cpp
  elf_symbol.cpp
  elf_symbol_definition.cpp
  elf_symbol_reference.cpp
  expat_handle.cpp
  expat_minidom.cpp
  expat_source.cpp
  fd_handle.cpp
  fd_sink.cpp
  fd_source.cpp
  file_cache.cpp
  file_handle.cpp
  gunzip_source.cpp
  hash.cpp
  memory_range_source.cpp
  os.cpp
  os_error_string.cpp
  os_exception.cpp
  os_exception_function.cpp
  os_exception_defaults.cpp
  os_current_directory.cpp
  os_readlink.cpp
  os_remove_directory_tree.cpp
  pg_exception.cpp
  pg_private.cpp
  pg_testdb.cpp
  pgconn_handle.cpp
  pgresult_handle.cpp
  regex_handle.cpp
  repomd.cpp
  repomd_primary.cpp
  repomd_primary_xml.cpp
  rpm_evr.cpp
  rpm_file_entry.cpp
  rpm_file_info.cpp
  rpm_load.cpp
  rpm_package_info.cpp
  rpm_parser.cpp
  rpm_parser_exception.cpp
  rpmtd_wrapper.cpp
  sink.cpp
  source.cpp
  source_sink.cpp
  string_sink.cpp
  string_source.cpp
  string_support.cpp
  subprocess.cpp
  task.cpp
  tee_sink.cpp
  url.cpp
  utf8.cpp
  vector_sink.cpp
  zlib.cpp
  zlib_inflate_exception.cpp
  ${CMAKE_CURRENT_BINARY_DIR}/error-constants.inc
)

add_library (SymbolDB
  database.cpp
  symboldb_download_repo.cpp
  symboldb_options.cpp
  symboldb_show_source_packages.cpp
  update_elf_closure.cpp
  ${CMAKE_CURRENT_BINARY_DIR}/schema.sql.inc
)

target_link_libraries (CXXLL
  -lcurl
  -ldl
  -lelf
  -lexpat
  -lnss3
  -lpq
  -lrpm -lrpmio
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
  symboldb.cpp
)

target_link_libraries (symboldb
  SymbolDB
)

install (TARGETS symboldb DESTINATION bin)

add_executable (pgtestshell
  pgtestshell.cpp
)

target_link_libraries (pgtestshell
  CXXLL
)

install (TARGETS pgtestshell DESTINATION bin)

add_executable (runtests
  test/runtests.cpp
  test/test-base16.cpp
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
  test/test-os.cpp
  test/test-os_exception.cpp
  test/test-pg_testdb.cpp
  test/test-regex_handle.cpp
  test/test-repomd.cpp
  test/test-rpm_load.cpp
  test/test-string_source.cpp
  test/test-string_support.cpp
  test/test-subprocess.cpp
  test/test-task.cpp
  test/test-utf8.cpp
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

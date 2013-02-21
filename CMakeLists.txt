cmake_minimum_required (VERSION 2.6)
project (symboldb)

if(CMAKE_COMPILER_IS_GNUCXX)
  set(CMAKE_CXX_FLAGS "-std=gnu++03 -O2 -g -Wall -W -fstack-protector -D_GNU_SOURCE -D_FORTIFY_SOURCE=2")
  set(CMAKE_EXE_LINKER_FLAGS "-g")
endif()

include_directories (
  /usr/include/nss3
  /usr/include/nspr4
  ${CMAKE_CURRENT_BINARY_DIR}
)

add_custom_command (
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/schema.sql.inc
  COMMAND xxd -i < ${PROJECT_SOURCE_DIR}/schema.sql > ${CMAKE_CURRENT_BINARY_DIR}/schema.sql.inc
  DEPENDS schema.sql
)

add_library (SymbolDB
  checksum.cpp
  cpio_reader.cpp
  curl_fetch_result.cpp
  curl_handle.cpp
  database.cpp
  database_exception.cpp
  download.cpp
  elf_exception.cpp
  elf_image.cpp
  elf_symbol.cpp
  elf_symbol_definition.cpp
  elf_symbol_reference.cpp
  expat_handle.cpp
  expat_minidom.cpp
  fd_handle.cpp
  fd_sink.cpp
  fd_source.cpp
  file_cache.cpp
  hash.cpp
  os.cpp
  os_exception.cpp
  os_exception_function.cpp
  os_exception_defaults.cpp
  os_current_directory.cpp
  os_readlink.cpp
  repomd.cpp
  rpm_evr.cpp
  rpm_file_entry.cpp
  rpm_file_info.cpp
  rpm_package_info.cpp
  rpm_parser.cpp
  rpm_parser_exception.cpp
  rpmtd_wrapper.cpp
  sink.cpp
  source.cpp
  source_sink.cpp
  string_support.cpp
  tee_sink.cpp
  url.cpp
  vector_sink.cpp
  zlib.cpp
  ${CMAKE_CURRENT_BINARY_DIR}/schema.sql.inc
)

target_link_libraries (SymbolDB
  -ldl
)

add_executable (symboldb
  symboldb.cpp
)

target_link_libraries (symboldb
  SymbolDB
  -lcurl
  -lrpm
  -lebl -lelf
  -lexpat
  -lnss3
  -lrpmio -lpq
  -lwwwcore -lwwwutils
  -lz
)

install (TARGETS symboldb DESTINATION bin)

add_executable (runtests
  runtests.cpp
  test-fd_handle.cpp
  test-fd_sink.cpp
  test-fd_source.cpp
  test-os.cpp
  test-os_exception.cpp
  test.cpp
)

target_link_libraries (runtests
  SymbolDB
)

CFLAGS = -O2 -g -Wall -W
CXXFLAGS = $(CFLAGS)
CXXFLAGS_ADD = -std=gnu++03
DEFINES = -D_GNU_SOURCE
INCLUDES = -I/usr/include/nss3 -I/usr/include/nspr4
LDFLAGS =
LIBS =  -lebl -lelf -ldl -lrpm -lrpmio -lpq -lcurl -lexpat -lwwwcore -lwwwutils -lz -lnss3

all: schema.sql.inc
	g++ $(DEFINES) $(INCLUDES) $(CXXFLAGS_ADD) $(CXXFLAGS) $(LDFLAGS) \
		-o symboldb \
		symboldb.cpp \
		cpio_reader.cpp rpm_parser.cpp rpm_parser_exception.cpp \
		rpm_file_entry.cpp rpm_file_info.cpp rpmtd_wrapper.cpp \
		rpm_package_info.cpp \
		curl_handle.cpp curl_fetch_result.cpp url.cpp download.cpp \
		expat_handle.cpp expat_minidom.cpp \
		checksum.cpp repomd.cpp \
		zlib.cpp os.cpp \
		string_support.cpp fd_handle.cpp hash.cpp file_cache.cpp \
		database.cpp database_exception.cpp \
		package_set_consolidator.cpp \
		elf_symbol.cpp \
		elf_symbol_definition.cpp \
		elf_symbol_reference.cpp \
		elf_exception.cpp \
		elf_image.cpp \
		$(LIBS)

schema.sql.inc: schema.sql
	xxd -i < $< > $@

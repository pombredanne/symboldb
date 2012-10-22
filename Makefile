CFLAGS = -O2 -g -Wall -W
CXXFLAGS = $(CFLAGS)
CXXFLAGS_ADD = -std=gnu++03
DEFINES = -D_GNU_SOURCE
LDFLAGS =
LIBS =  -lebl -lelf -ldl -lrpm -lrpmio -lpq

all:
	g++ $(DEFINES) $(CXXFLAGS_ADD) $(CXXFLAGS) $(LDFLAGS) -o symboldb \
		symboldb.cpp find-symbols.cpp find_symbols_exception.cpp \
		cpio_reader.cpp rpm_parser.cpp rpm_parser_exception.cpp \
		rpm_file_entry.cpp rpm_file_info.cpp rpmtd_wrapper.cpp \
		rpm_package_info.cpp \
		database.cpp database_exception.cpp \
		$(LIBS)


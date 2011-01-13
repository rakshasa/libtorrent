TESTS = LibTorrentTest
check_PROGRAMS = $(TESTS)
LibTorrentTest_LDADD = \
	../src/libtorrent.la

LibTorrentTest_SOURCES = \
	rak/allocators_test.cc \
	rak/allocators_test.h \
	rak/ranges_test.cc \
	rak/ranges_test.h \
	torrent/object_test.cc \
	torrent/object_test.h \
	torrent/object_test_utils.cc \
	torrent/object_test_utils.h \
	torrent/object_static_map_test.cc \
	torrent/object_static_map_test.h \
	torrent/object_stream_test.cc \
	torrent/object_stream_test.h \
	main.cc

LibTorrentTest_CXXFLAGS = $(CPPUNIT_CFLAGS)
LibTorrentTest_LDFLAGS = $(CPPUNIT_LIBS)  -ldl

INCLUDES = -I$(srcdir) -I$(top_srcdir) -I$(top_srcdir)/src

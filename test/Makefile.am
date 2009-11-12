TESTS = LibTorrentTest
check_PROGRAMS = $(TESTS)
LibTorrentTest_SOURCES = \
	torrent/object_stream_test.cc \
	torrent/object_stream_test.h \
	torrent/task_manager_test.cc \
	torrent/task_manager_test.h \
	main.cc
LibTorrentTest_CXXFLAGS = $(CPPUNIT_CFLAGS)
LibTorrentTest_LDFLAGS = $(CPPUNIT_LIBS)  -ldl ../src/.libs/libtorrent.dylib

INCLUDES = -I$(srcdir) -I$(top_srcdir) -I$(top_srcdir)/../src

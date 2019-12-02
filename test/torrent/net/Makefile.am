TESTS = LibTorrentTestTorrentNet
AUTOMAKE_OPTIONS = subdir-objects

check_PROGRAMS = $(TESTS)
LibTorrentTestTorrentNet_LDADD = \
	../../../src/libtorrent.la \
	../../../src/torrent/libsub_torrent.la \
	../../../src/torrent/data/libsub_torrentdata.la \
	../../../src/torrent/download/libsub_torrentdownload.la \
	../../../src/torrent/peer/libsub_torrentpeer.la \
	../../../src/data/libsub_data.la \
	../../../src/dht/libsub_dht.la \
	../../../src/net/libsub_net.la \
	../../../src/protocol/libsub_protocol.la \
	../../../src/download/libsub_download.la \
	../../../src/tracker/libsub_tracker.la \
	../../../src/utils/libsub_utils.la \
	../../../src/torrent/utils/libsub_torrentutils.la

LibTorrentTestTorrentNet_SOURCES = \
	../../helpers/expect_fd.h \
	../../helpers/expect_utils.h \
	../../helpers/mock_compare.h \
	../../helpers/mock_function.cc \
	../../helpers/mock_function.h \
	../../helpers/network.h \
	../../helpers/progress_listener.cc \
	../../helpers/progress_listener.h \
	../../helpers/test_fixture.cc \
	../../helpers/test_fixture.h \
	\
	test_address_info.cc \
	test_address_info.h \
	test_fd.cc \
	test_fd.h \
	test_socket_address.cc \
	test_socket_address.h \
	\
	../../main.cc

LibTorrentTestTorrentNet_CXXFLAGS = $(CPPUNIT_CFLAGS)
LibTorrentTestTorrentNet_LDFLAGS = $(CPPUNIT_LIBS) -ldl

AM_CPPFLAGS = -I$(top_srcdir) -I$(top_srcdir)/src -I$(top_srcdir)/test

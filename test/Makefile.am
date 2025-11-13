TESTS = \
	LibTorrent_Test_Torrent_Net \
	LibTorrent_Test_Torrent_Utils \
	LibTorrent_Test_Torrent \
	LibTorrent_Test_Data \
	LibTorrent_Test_Net \
	LibTorrent_Test_Tracker \
	LibTorrent_Test

check_PROGRAMS = $(TESTS)

# This can cause duplicate symbols, so export anything that causes issues.

# LibTorrent_Test_LDADD = ../src/libtorrent.la
LibTorrent_Test_LDADD = \
	../src/libtorrent.la \
	../src/libtorrent_other.la \
	../src/torrent/libtorrent_torrent.la

LibTorrent_Test_Torrent_Net_LDADD = $(LibTorrent_Test_LDADD)
LibTorrent_Test_Torrent_Utils_LDADD = $(LibTorrent_Test_LDADD)
LibTorrent_Test_Torrent_LDADD = $(LibTorrent_Test_LDADD)
LibTorrent_Test_Data_LDADD = $(LibTorrent_Test_LDADD)
LibTorrent_Test_Net_LDADD = $(LibTorrent_Test_LDADD)
LibTorrent_Test_Tracker_LDADD = $(LibTorrent_Test_LDADD)

LibTorrent_Test_Common = \
	main.cc \
	helpers/expect_fd.h \
	helpers/expect_utils.h \
	helpers/mock_compare.h \
	helpers/mock_function.cc \
	helpers/mock_function.h \
	helpers/network.cc \
	helpers/network.h \
	helpers/progress_listener.cc \
	helpers/progress_listener.h \
	helpers/protectors.cc \
	helpers/protectors.h \
	helpers/test_fixture.cc \
	helpers/test_fixture.h \
	helpers/test_main_thread.cc \
	helpers/test_main_thread.h \
	helpers/test_thread.cc \
	helpers/test_thread.h \
	helpers/test_utils.h \
	helpers/tracker_test.cc \
	helpers/tracker_test.h \
	helpers/utils.h

LibTorrent_Test_Torrent_Net_SOURCES = $(LibTorrent_Test_Common) \
	torrent/net/test_address_info.cc \
	torrent/net/test_address_info.h \
	torrent/net/test_fd.cc \
	torrent/net/test_fd.h \
	torrent/net/test_socket_address.cc \
	torrent/net/test_socket_address.h

LibTorrent_Test_Torrent_Utils_SOURCES = $(LibTorrent_Test_Common) \
	torrent/utils/test_extents.cc \
	torrent/utils/test_extents.h \
	torrent/utils/test_log.cc \
	torrent/utils/test_log.h \
	torrent/utils/test_log_buffer.cc \
	torrent/utils/test_log_buffer.h \
	torrent/utils/test_option_strings.cc \
	torrent/utils/test_option_strings.h \
	torrent/utils/test_queue_buckets.cc \
	torrent/utils/test_queue_buckets.h \
	torrent/utils/test_signal_bitfield.cc \
	torrent/utils/test_signal_bitfield.h \
	torrent/utils/test_signal_interrupt.cc \
	torrent/utils/test_signal_interrupt.h \
	torrent/utils/test_thread_base.cc \
	torrent/utils/test_thread_base.h \
	torrent/utils/test_uri_parser.cc \
	torrent/utils/test_uri_parser.h

LibTorrent_Test_Torrent_SOURCES = $(LibTorrent_Test_Common) \
	torrent/object_test.cc \
	torrent/object_test.h \
	torrent/object_test_utils.cc \
	torrent/object_test_utils.h \
	torrent/object_static_map_test.cc \
	torrent/object_static_map_test.h \
	torrent/object_stream_test.cc \
	torrent/object_stream_test.h \
	torrent/test_tracker_controller.cc \
	torrent/test_tracker_controller.h \
	torrent/test_tracker_controller_features.cc \
	torrent/test_tracker_controller_features.h \
	torrent/test_tracker_controller_requesting.cc \
	torrent/test_tracker_controller_requesting.h \
	torrent/test_tracker_list.cc \
	torrent/test_tracker_list.h \
	torrent/test_tracker_list_features.cc \
	torrent/test_tracker_list_features.h \
	torrent/test_tracker_timeout.cc \
	torrent/test_tracker_timeout.h

LibTorrent_Test_Data_SOURCES = $(LibTorrent_Test_Common) \
	data/test_chunk_list.cc \
	data/test_chunk_list.h \
	data/test_hash_check_queue.cc \
	data/test_hash_check_queue.h \
	data/test_hash_queue.cc \
	data/test_hash_queue.h

LibTorrent_Test_Net_SOURCES = $(LibTorrent_Test_Common)

LibTorrent_Test_Tracker_SOURCES = $(LibTorrent_Test_Common) \
	tracker/test_tracker_http.cc \
	tracker/test_tracker_http.h

LibTorrent_Test_SOURCES = $(LibTorrent_Test_Common) \
	\
	rak/allocators_test.cc \
	rak/allocators_test.h \
	rak/ranges_test.cc \
	rak/ranges_test.h \
	\
	protocol/test_request_list.cc \
	protocol/test_request_list.h

LibTorrent_Test_Torrent_Net_CXXFLAGS = $(CPPUNIT_CFLAGS)
LibTorrent_Test_Torrent_Net_LDFLAGS = $(CPPUNIT_LIBS)
LibTorrent_Test_Torrent_Utils_CXXFLAGS = $(CPPUNIT_CFLAGS)
LibTorrent_Test_Torrent_Utils_LDFLAGS = $(CPPUNIT_LIBS)
LibTorrent_Test_Torrent_CXXFLAGS = $(CPPUNIT_CFLAGS)
LibTorrent_Test_Torrent_LDFLAGS = $(CPPUNIT_LIBS)
LibTorrent_Test_Data_CXXFLAGS = $(CPPUNIT_CFLAGS)
LibTorrent_Test_Data_LDFLAGS = $(CPPUNIT_LIBS)
LibTorrent_Test_Net_CXXFLAGS = $(CPPUNIT_CFLAGS)
LibTorrent_Test_Net_LDFLAGS = $(CPPUNIT_LIBS)
LibTorrent_Test_Tracker_CXXFLAGS = $(CPPUNIT_CFLAGS)
LibTorrent_Test_Tracker_LDFLAGS = $(CPPUNIT_LIBS)
LibTorrent_Test_CXXFLAGS = $(CPPUNIT_CFLAGS)
LibTorrent_Test_LDFLAGS = $(CPPUNIT_LIBS)

AM_CPPFLAGS = -I$(srcdir) -I$(top_srcdir) -I$(top_srcdir)/src

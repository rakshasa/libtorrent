TESTS = \
	LibTorrent_Test_Torrent_Net \
	LibTorrent_Test_Net \
	LibTorrent_Test

check_PROGRAMS = $(TESTS)

LibTorrent_Test_LDADD = \
	../src/libtorrent.la \
	../src/libtorrent_other.la \
	../src/torrent/libtorrent_torrent.la

LibTorrent_Test_Net_LDADD = $(LibTorrent_Test_LDADD)
LibTorrent_Test_Torrent_Net_LDADD = $(LibTorrent_Test_LDADD)

# LibTorrent_Test_SOURCES = \
# 	helpers/expect_fd.h \
# 	helpers/expect_utils.h \
# 	helpers/mock_compare.h \
# 	helpers/mock_function.cc \
# 	helpers/mock_function.h \
# 	helpers/network.h \
# 	helpers/progress_listener.cc \
# 	helpers/progress_listener.h \
# 	helpers/test_fixture.cc \
# 	helpers/test_fixture.h

LibTorrent_Test_Torrent_Net_SOURCES = \
	main.cc \
	helpers/expect_fd.h \
	helpers/expect_utils.h \
	helpers/mock_compare.h \
	helpers/mock_function.cc \
	helpers/mock_function.h \
	helpers/network.h \
	helpers/progress_listener.cc \
	helpers/progress_listener.h \
	helpers/test_fixture.cc \
	helpers/test_fixture.h \
	\
	torrent/net/test_address_info.cc \
	torrent/net/test_address_info.h \
	torrent/net/test_fd.cc \
	torrent/net/test_fd.h \
	torrent/net/test_socket_address.cc \
	torrent/net/test_socket_address.h

LibTorrent_Test_Net_SOURCES = \
	main.cc \
	helpers/expect_fd.h \
	helpers/expect_utils.h \
	helpers/mock_compare.h \
	helpers/mock_function.cc \
	helpers/mock_function.h \
	helpers/network.h \
	helpers/progress_listener.cc \
	helpers/progress_listener.h \
	helpers/test_fixture.cc \
	helpers/test_fixture.h \
	\
	net/test_socket_listen.cc \
	net/test_socket_listen.h

LibTorrent_Test_SOURCES = \
	main.cc \
	helpers/expect_fd.h \
	helpers/expect_utils.h \
	helpers/mock_compare.h \
	helpers/mock_function.cc \
	helpers/mock_function.h \
	helpers/network.h \
	helpers/progress_listener.cc \
	helpers/progress_listener.h \
	helpers/test_fixture.cc \
	helpers/test_fixture.h \
	\
	../src/thread_disk.cc \
	../src/thread_disk.h \
	\
	rak/allocators_test.cc \
	rak/allocators_test.h \
	rak/ranges_test.cc \
	rak/ranges_test.h \
	data/chunk_list_test.cc \
	data/chunk_list_test.h \
	data/hash_check_queue_test.cc \
	data/hash_check_queue_test.h \
	data/hash_queue_test.cc \
	data/hash_queue_test.h \
	\
	protocol/test_request_list.cc \
	protocol/test_request_list.h \
	\
	torrent/utils/log_test.cc \
	torrent/utils/log_test.h \
	torrent/utils/option_strings_test.cc \
	torrent/utils/option_strings_test.h \
	torrent/utils/test_extents.cc \
	torrent/utils/test_extents.h \
	torrent/utils/test_log_buffer.cc \
	torrent/utils/test_log_buffer.h \
	torrent/utils/test_queue_buckets.cc \
	torrent/utils/test_queue_buckets.h \
	torrent/utils/test_uri_parser.cc \
	torrent/utils/test_uri_parser.h \
	torrent/utils/signal_bitfield_test.cc \
	torrent/utils/signal_bitfield_test.h \
	torrent/utils/thread_base_test.cc \
	torrent/utils/thread_base_test.h \
	\
	torrent/http_test.cc \
	torrent/http_test.h \
	torrent/object_test.cc \
	torrent/object_test.h \
	torrent/object_test_utils.cc \
	torrent/object_test_utils.h \
	torrent/object_static_map_test.cc \
	torrent/object_static_map_test.h \
	torrent/object_stream_test.cc \
	torrent/object_stream_test.h \
	torrent/tracker_controller_test.cc \
	torrent/tracker_controller_test.h \
	torrent/tracker_controller_features.cc \
	torrent/tracker_controller_features.h \
	torrent/tracker_controller_requesting.cc \
	torrent/tracker_controller_requesting.h \
	torrent/tracker_list_test.cc \
	torrent/tracker_list_test.h \
	torrent/tracker_list_features_test.cc \
	torrent/tracker_list_features_test.h \
	torrent/tracker_timeout_test.cc \
	torrent/tracker_timeout_test.h \
	\
	tracker/tracker_http_test.cc \
	tracker/tracker_http_test.h

LibTorrent_Test_Torrent_Net_CXXFLAGS = $(CPPUNIT_CFLAGS)
LibTorrent_Test_Torrent_Net_LDFLAGS = $(CPPUNIT_LIBS) -ldl
LibTorrent_Test_Net_CXXFLAGS = $(CPPUNIT_CFLAGS)
LibTorrent_Test_Net_LDFLAGS = $(CPPUNIT_LIBS) -ldl
LibTorrent_Test_CXXFLAGS = $(CPPUNIT_CFLAGS)
LibTorrent_Test_LDFLAGS = $(CPPUNIT_LIBS) -ldl

AM_CPPFLAGS = -I$(srcdir) -I$(top_srcdir) -I$(top_srcdir)/src

noinst_LTLIBRARIES = libtorrent_torrent.la

libtorrent_torrent_la_SOURCES = \
	data/block.cc \
	data/block.h \
	data/block_failed.h \
	data/block_list.cc \
	data/block_list.h \
	data/block_transfer.h \
	data/chunk_utils.cc \
	data/chunk_utils.h \
	data/download_data.cc \
	data/download_data.h \
	data/file.cc \
	data/file.h \
	data/file_list.cc \
	data/file_list.h \
	data/file_list_iterator.cc \
	data/file_list_iterator.h \
	data/file_manager.cc \
	data/file_manager.h \
	data/file_utils.cc \
	data/file_utils.h \
	data/piece.h \
	data/transfer_list.cc \
	data/transfer_list.h \
\
	download/choke_group.cc \
	download/choke_group.h \
	download/choke_queue.cc \
	download/choke_queue.h \
	download/download_manager.cc \
	download/download_manager.h \
	download/group_entry.h \
	download/resource_manager.cc \
	download/resource_manager.h \
\
	net/address_info.cc \
	net/address_info.h \
	net/fd.cc \
	net/fd.h \
	net/socket_address.cc \
	net/socket_address.h \
	net/socket_address_key.cc \
	net/socket_address_key.h \
	net/socket_event.cc \
	net/socket_event.h \
	net/types.h \
	net/utils.cc \
	net/utils.h \
\
	peer/choke_status.h \
	peer/client_info.cc \
	peer/client_info.h \
	peer/client_list.cc \
	peer/client_list.h \
	peer/connection_list.cc \
	peer/connection_list.h \
	peer/peer.cc \
	peer/peer.h \
        peer/peer_info.cc \
        peer/peer_info.h \
	peer/peer_list.cc \
	peer/peer_list.h \
\
	utils/directory_events.cc \
	utils/directory_events.h \
	utils/extents.h \
	utils/log.cc \
	utils/log.h \
	utils/log_buffer.cc \
	utils/log_buffer.h \
	utils/option_strings.cc \
	utils/option_strings.h \
	utils/random.cc \
	utils/random.h \
	utils/ranges.h \
	utils/resume.cc \
	utils/resume.h \
	utils/signal_bitfield.cc \
	utils/signal_bitfield.h \
	utils/thread_base.cc \
	utils/thread_base.h \
	utils/thread_interrupt.cc \
	utils/thread_interrupt.h \
	utils/uri_parser.cc \
	utils/uri_parser.h \
\
	bitfield.cc \
	bitfield.h \
	chunk_manager.cc \
	chunk_manager.h \
	common.h \
	connection_manager.cc \
	connection_manager.h \
	dht_manager.cc \
	dht_manager.h \
	download.cc \
	download.h \
	download_info.h \
	error.cc \
	error.h \
	event.cc \
	event.h \
	exceptions.cc \
	exceptions.h \
	hash_string.cc \
	hash_string.h \
	http.cc \
	http.h \
	object.cc \
	object.h \
	object_raw_bencode.h \
	object_static_map.cc \
	object_static_map.h \
	object_stream.cc \
	object_stream.h \
	path.cc \
	path.h \
	poll.h \
	poll_epoll.cc \
	poll_epoll.h \
	poll_kqueue.h \
	poll_kqueue.cc \
	poll_select.h \
	poll_select.cc \
	rate.cc \
	rate.h \
	throttle.cc \
	throttle.h \
	torrent.cc \
	torrent.h \
	tracker.cc \
	tracker.h \
	tracker_controller.cc \
	tracker_controller.h \
	tracker_list.cc \
	tracker_list.h

AM_CPPFLAGS = -I$(srcdir) -I$(srcdir)/.. -I$(top_srcdir)

libtorrent_torrent_data_includedir = $(includedir)/torrent/data
libtorrent_torrent_data_include_HEADERS = \
	data/block.h \
	data/block_list.h \
	data/block_transfer.h \
	data/chunk_utils.h \
	data/download_data.h \
	data/file.h \
	data/file_list.h \
	data/file_list_iterator.h \
	data/file_manager.h \
	data/file_utils.h \
	data/piece.h \
	data/transfer_list.h

libtorrent_torrent_download_includedir = $(includedir)/torrent/download
libtorrent_torrent_download_include_HEADERS = \
	download/choke_group.h \
	download/choke_queue.h \
	download/download_manager.h \
	download/group_entry.h \
	download/resource_manager.h

libtorrent_torrent_net_includedir = $(includedir)/torrent/net
libtorrent_torrent_net_include_HEADERS = \
	net/address_info.h \
	net/fd.h \
	net/socket_address.h \
	net/socket_address_key.h \
	net/socket_event.h \
	net/types.h \
	net/utils.h

libtorrent_torrent_peer_includedir = $(includedir)/torrent/peer
libtorrent_torrent_peer_include_HEADERS = \
	peer/choke_status.h \
	peer/client_info.h \
	peer/client_list.h \
	peer/connection_list.h \
	peer/peer.h \
        peer/peer_info.h \
	peer/peer_list.h

libtorrent_torrent_utils_includedir = $(includedir)/torrent/utils
libtorrent_torrent_utils_include_HEADERS = \
	utils/directory_events.h \
	utils/extents.h \
	utils/log.h \
	utils/log_buffer.h \
	utils/option_strings.h \
	utils/ranges.h \
	utils/resume.h \
	utils/signal_bitfield.h \
	utils/thread_base.h \
	utils/thread_interrupt.h \
	utils/uri_parser.h

libtorrent_torrent_includedir = $(includedir)/torrent
libtorrent_torrent_include_HEADERS = \
	bitfield.h \
	chunk_manager.h \
	common.h \
	connection_manager.h \
	dht_manager.h \
	download.h \
	download_info.h \
	error.h \
	exceptions.h \
	event.h \
	hash_string.h \
	http.h \
	object.h \
	object_raw_bencode.h \
	object_static_map.h \
	object_stream.h \
	path.h \
	poll.h \
	poll_epoll.h \
	poll_kqueue.h \
	poll_select.h \
	rate.h \
	throttle.h \
	torrent.h \
	tracker.h \
	tracker_controller.h \
	tracker_list.h

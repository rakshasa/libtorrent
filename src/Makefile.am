SUBDIRS = torrent

lib_LTLIBRARIES = libtorrent.la
noinst_LTLIBRARIES = \
	libtorrent_other.la

libtorrent_la_LDFLAGS = -version-info $(LIBTORRENT_INTERFACE_VERSION_INFO)
libtorrent_la_LIBADD = \
	torrent/libtorrent_torrent.la \
	libtorrent_other.la

libtorrent_la_SOURCES = \
	manager.cc \
	manager.h \
	thread_main.cc \
	thread_main.h

libtorrent_other_la_SOURCES = \
	data/chunk.cc \
	data/chunk.h \
	data/chunk_handle.h \
	data/chunk_iterator.h \
	data/chunk_list.cc \
	data/chunk_list.h \
	data/chunk_list_node.h \
	data/chunk_part.cc \
	data/chunk_part.h \
	data/hash_check_queue.cc \
	data/hash_check_queue.h \
	data/hash_chunk.cc \
	data/hash_chunk.h \
	data/hash_queue.cc \
	data/hash_queue.h \
	data/hash_queue_node.cc \
	data/hash_queue_node.h \
	data/hash_torrent.cc \
	data/hash_torrent.h \
	data/memory_chunk.cc \
	data/memory_chunk.h \
	data/socket_file.cc \
	data/socket_file.h \
	data/thread_disk.cc \
	data/thread_disk.h \
	\
	dht/dht_bucket.cc \
	dht/dht_bucket.h \
	dht/dht_hash_map.h \
	dht/dht_node.cc \
	dht/dht_node.h \
	dht/dht_router.cc \
	dht/dht_router.h \
	dht/dht_server.cc \
	dht/dht_server.h \
	dht/dht_tracker.cc \
	dht/dht_tracker.h \
	dht/dht_transaction.cc \
	dht/dht_transaction.h \
	\
	download/available_list.cc \
	download/available_list.h \
	download/chunk_selector.cc \
	download/chunk_selector.h \
	download/chunk_statistics.cc \
	download/chunk_statistics.h \
	download/delegator.cc \
	download/delegator.h \
	download/download_constructor.cc \
	download/download_constructor.h \
	download/download_main.cc \
	download/download_main.h \
	download/download_wrapper.cc \
	download/download_wrapper.h \
	\
	net/address_list.cc \
	net/address_list.h \
	net/data_buffer.h \
	net/curl_get.cc \
	net/curl_get.h \
	net/curl_socket.cc \
	net/curl_socket.h \
	net/curl_stack.cc \
	net/curl_stack.h \
	net/listen.cc \
	net/listen.h \
	net/protocol_buffer.h \
	net/socket_base.cc \
	net/socket_base.h \
	net/socket_datagram.cc \
	net/socket_datagram.h \
	net/socket_fd.cc \
	net/socket_fd.h \
	net/socket_listen.cc \
	net/socket_listen.h \
	net/socket_set.cc \
	net/socket_set.h \
	net/socket_stream.cc \
	net/socket_stream.h \
	net/thread_net.cc \
	net/thread_net.h \
	net/throttle_internal.cc \
	net/throttle_internal.h \
	net/throttle_list.cc \
	net/throttle_list.h \
	net/throttle_node.h \
	net/udns_resolver.cc \
	net/udns_resolver.h \
	net/udns_library.cc \
	net/udns_library.h \
	\
	net/udns/config.h \
	net/udns/udns.h \
	\
	protocol/encryption_info.h \
	protocol/extensions.cc \
	protocol/extensions.h \
	protocol/handshake.cc \
	protocol/handshake.h \
	protocol/handshake_encryption.cc \
	protocol/handshake_encryption.h \
	protocol/handshake_manager.cc \
	protocol/handshake_manager.h \
	protocol/initial_seed.cc \
	protocol/initial_seed.h \
	protocol/peer_chunks.h \
	protocol/peer_connection_base.cc \
	protocol/peer_connection_base.h \
	protocol/peer_connection_leech.cc \
	protocol/peer_connection_leech.h \
	protocol/peer_connection_metadata.cc \
	protocol/peer_connection_metadata.h \
	protocol/peer_factory.cc \
	protocol/peer_factory.h \
	protocol/protocol_base.h \
	protocol/request_list.cc \
	protocol/request_list.h \
	\
	tracker/thread_tracker.cc \
	tracker/thread_tracker.h \
	tracker/tracker_controller.cc \
	tracker/tracker_controller.h \
	tracker/tracker_dht.cc \
	tracker/tracker_dht.h \
	tracker/tracker_http.cc \
	tracker/tracker_http.h \
	tracker/tracker_list.cc \
	tracker/tracker_list.h \
	tracker/tracker_udp.cc \
	tracker/tracker_udp.h \
	tracker/tracker_worker.cc \
	tracker/tracker_worker.h \
	\
	utils/diffie_hellman.cc \
	utils/diffie_hellman.h \
	utils/functional.h \
	utils/instrumentation.cc \
	utils/instrumentation.h \
	utils/rc4.h \
	utils/sha1.h \
	utils/signal_interrupt.cc \
	utils/signal_interrupt.h \
	utils/thread_internal.h \
	utils/queue_buckets.h

AM_CPPFLAGS = -I$(srcdir) -I$(top_srcdir)

EXTRA_DIST= \
	net/udns/dnsget.c \
	net/udns/ex-rdns.c \
	net/udns/getopt.c \
	net/udns/inet_XtoX.c \
	net/udns/rblcheck.c \
	net/udns/udns.h \
	net/udns/udns_XtoX.c \
	net/udns/udns_bl.c \
	net/udns/udns_codes.c \
	net/udns/udns_dn.c \
	net/udns/udns_dntosp.c \
	net/udns/udns_init.c \
	net/udns/udns_jran.c \
	net/udns/udns_misc.c \
	net/udns/udns_parse.c \
	net/udns/udns_resolver.c \
	net/udns/udns_rr_a.c \
	net/udns/udns_rr_mx.c \
	net/udns/udns_rr_naptr.c \
	net/udns/udns_rr_ptr.c \
	net/udns/udns_rr_srv.c \
	net/udns/udns_rr_txt.c

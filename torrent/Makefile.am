lib_LTLIBRARIES = libtorrent.la

libtorrent_la_LDFLAGS = -version-info $(LIBTORRENT_INTERFACE_VERSION_INFO)
libtorrent_la_SOURCES = \
	torrent/download.cc \
	torrent/download.h \
	torrent/entry.cc \
	torrent/entry.h \
	torrent/exceptions.h \
	torrent/peer.cc \
	torrent/peer.h \
	torrent/torrent.cc \
	torrent/torrent.h \
	content/content.cc \
	content/content.h \
	content/content_file.h \
	content/delegator_piece.h \
	content/delegator_select.cc \
	content/delegator_select.h \
	content/priority.cc \
	content/priority.h \
	data/file.cc \
	data/file.h \
	data/file_chunk.cc \
	data/file_chunk.h \
	data/hash_chunk.cc \
	data/hash_chunk.h \
	data/hash_compute.h \
	data/hash_queue.cc \
	data/hash_queue.h \
	data/hash_torrent.cc \
	data/hash_torrent.h \
	data/path.cc \
	data/path.h \
	data/storage.cc \
	data/storage.h \
	data/storage_chunk.cc \
	data/storage_chunk.h \
	data/storage_consolidator.cc \
	data/storage_consolidator.h \
	data/storage_file.h \
	net/listen.cc \
	net/listen.h \
	parse/parse_info.cc \
	parse/parse_info.h \
	tracker/tracker_control.cc \
	tracker/tracker_control.h \
	tracker/tracker_http.cc \
	tracker/tracker_http.h \
	tracker/tracker_state.h \
	url/curl_get.cc \
	url/curl_get.h \
	url/curl_stack.cc \
	url/curl_stack.h \
	url/http.cc \
	url/http.h \
	bencode.cc \
	bencode.h \
        bitfield.cc \
        bitfield.h \
	bitfield_counter.h \
        delegator.cc \
        delegator.h \
        download_main.cc \
        download_main.h \
	download_state.cc \
	download_state.h \
        general.cc \
        general.h \
        peer_info.cc \
        peer_info.h \
        peer_handshake.cc \
        peer_handshake.h \
        peer_connection.cc \
        peer_connection.h \
        peer_connection_extra.cc \
	peer_connection_sub.h \
	peer_wrapper.h \
	piece.h \
	rate.cc \
	rate.h \
	settings.cc \
	settings.h \
        socket_base.cc \
        socket_base.h \
	timer.h \
	throttle.cc \
	throttle.h \
	throttle_control.cc \
	throttle_control.h \
        service.cc \
        service.h

libtorrentincludedir = $(includedir)/torrent
libtorrentinclude_HEADERS = \
	torrent/common.h \
	torrent/download.h \
	torrent/exceptions.h \
	torrent/entry.h \
	torrent/peer.h \
	torrent/torrent.h \
	url/http.h

DISTCLEANFILES = *~ */*~

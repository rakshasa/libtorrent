lib_LTLIBRARIES = libtorrent.la

libtorrent_la_LDFLAGS =
libtorrent_la_SOURCES = \
	content/content.cc \
	content/content.h \
	content/content_file.h \
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
        download.cc \
        download.h \
	download_state.cc \
	download_state.h \
        general.cc \
        general.h \
        peer.cc \
        peer.h \
        peer_handshake.cc \
        peer_handshake.h \
        peer_connection.cc \
        peer_connection.h \
        peer_connection_extra.cc \
	peer_connection_sub.h \
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
        torrent.cc \
        torrent.h \
        service.cc \
        service.h

libtorrentincludedir = $(includedir)/torrent
libtorrentinclude_HEADERS = \
	torrent.h \
	exceptions.h \
	url/http.h

DISTCLEANFILES = *~ */*~

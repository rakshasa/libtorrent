lib_LTLIBRARIES = libtorrent.la

libtorrent_la_LDFLAGS =
libtorrent_la_SOURCES = \
	tracker/tracker_control.cc \
	tracker/tracker_control.h \
	tracker/tracker_http.cc \
	tracker/tracker_http.h \
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
        chunk.cc \
        chunk.h \
        delegator.cc \
        delegator.h \
        download.cc \
        download.h \
	download_state.cc \
	download_state.h \
        files.cc \
        files.h \
	files_algo.h \
        files_check.cc \
        files_check.h \
        general.cc \
        general.h \
        listen.cc \
        listen.h \
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
        storage.cc \
        storage.h \
        storage_block.cc \
        storage_block.h \
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

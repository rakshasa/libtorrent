SUBDIRS = \
	torrent \
	content \
	data \
	download \
	net \
	parse \
	peer \
	tracker \
	utils

lib_LTLIBRARIES = libtorrent.la

libtorrent_la_LDFLAGS = -version-info $(LIBTORRENT_INTERFACE_VERSION_INFO)
libtorrent_la_LIBADD = \
	$(top_srcdir)/src/torrent/libsub_torrent.la \
	$(top_srcdir)/src/content/libsub_content.la \
	$(top_srcdir)/src/data/libsub_data.la \
	$(top_srcdir)/src/download/libsub_download.la \
	$(top_srcdir)/src/net/libsub_net.la \
	$(top_srcdir)/src/parse/libsub_parse.la \
	$(top_srcdir)/src/peer/libsub_peer.la \
	$(top_srcdir)/src/tracker/libsub_tracker.la \
	$(top_srcdir)/src/utils/libsub_utils.la

libtorrent_la_SOURCES = \
        peer_connection.cc \
        peer_connection.h \
	settings.cc \
	settings.h

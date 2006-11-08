SUBDIRS = \
	torrent \
	data \
	download \
	net \
	protocol \
	tracker \
	utils

lib_LTLIBRARIES = libtorrent.la

libtorrent_la_LDFLAGS = -version-info $(LIBTORRENT_INTERFACE_VERSION_INFO)
libtorrent_la_LIBADD = \
	$(top_srcdir)/src/torrent/libsub_torrent.la \
	$(top_srcdir)/src/torrent/data/libsub_torrentdata.la \
	$(top_srcdir)/src/torrent/peer/libsub_torrentpeer.la \
	$(top_srcdir)/src/data/libsub_data.la \
	$(top_srcdir)/src/download/libsub_download.la \
	$(top_srcdir)/src/net/libsub_net.la \
	$(top_srcdir)/src/protocol/libsub_protocol.la \
	$(top_srcdir)/src/tracker/libsub_tracker.la \
	$(top_srcdir)/src/utils/libsub_utils.la

libtorrent_la_SOURCES = \
	globals.cc \
	globals.h \
	manager.cc \
	manager.h \
	resource_manager.cc \
	resource_manager.h

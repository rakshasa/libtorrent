SUBDIRS = \
	torrent \
	data \
	dht \
	download \
	net \
	protocol \
	tracker \
	utils

lib_LTLIBRARIES = libtorrent.la

libtorrent_la_LDFLAGS = -version-info $(LIBTORRENT_INTERFACE_VERSION_INFO)
libtorrent_la_LIBADD = \
	torrent/libsub_torrent.la \
	torrent/data/libsub_torrentdata.la \
	torrent/peer/libsub_torrentpeer.la \
	data/libsub_data.la \
	dht/libsub_dht.la \
	download/libsub_download.la \
	net/libsub_net.la \
	protocol/libsub_protocol.la \
	tracker/libsub_tracker.la \
	utils/libsub_utils.la

libtorrent_la_SOURCES = \
	globals.cc \
	globals.h \
	manager.cc \
	manager.h \
	resource_manager.cc \
	resource_manager.h

INCLUDES = -I$(srcdir) -I$(top_srcdir)

noinst_LTLIBRARIES = libsub_torrentpeer.la

libsub_torrentpeer_la_SOURCES = \
	peer.cc \
	peer.h \
        peer_info.cc \
        peer_info.h \
	peer_list.cc \
	peer_list.h

INCLUDES = -I$(srcdir) -I$(srcdir)/.. -I$(srcdir)/../.. -I$(top_srcdir)

libtorrentincludedir = $(includedir)/torrent/peer
libtorrentinclude_HEADERS = \
	peer.h \
        peer_info.h \
	peer_list.h

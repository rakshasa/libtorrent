noinst_LTLIBRARIES = libsub_torrent.la

libsub_torrent_la_SOURCES = \
	bencode.cc \
	bencode.h \
	download.cc \
	download.h \
	entry.cc \
	entry.h \
	exceptions.h \
	http.cc \
	http.h \
	peer.cc \
	peer.h \
	torrent.cc \
	torrent.h \
	tracker.cc \
	tracker.h

INCLUDES = -I$(srcdir) -I$(srcdir)/.. -I$(top_srcdir)

libtorrentincludedir = $(includedir)/torrent
libtorrentinclude_HEADERS = \
	bencode.h \
	common.h \
	download.h \
	exceptions.h \
	entry.h \
	http.h \
	peer.h \
	torrent.h \
	tracker.h

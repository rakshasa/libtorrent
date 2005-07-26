noinst_LTLIBRARIES = libsub_torrent.la

libsub_torrent_la_SOURCES = \
	bencode.cc \
	bencode.h \
	download.cc \
	download.h \
	entry.cc \
	entry.h \
	event.h \
	exceptions.h \
	http.cc \
	http.h \
	peer.cc \
	peer.h \
	poll.h \
	poll_epoll.cc \
	poll_epoll.h \
	poll_select.h \
	poll_select.cc \
	rate.cc \
	rate.h \
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
	event.h \
	http.h \
	peer.h \
	poll.h \
	poll_epoll.h \
	poll_select.h \
	rate.h \
	torrent.h \
	tracker.h

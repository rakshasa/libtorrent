noinst_LTLIBRARIES = libsub_torrent.la

libsub_torrent_la_SOURCES = \
	download.cc \
	download.h \
	connection_manager.cc \
	connection_manager.h \
	entry.cc \
	entry.h \
	event.h \
	exceptions.cc \
	exceptions.h \
	http.cc \
	http.h \
	object.cc \
	object.h \
	object_stream.cc \
	object_stream.h \
	peer.cc \
	peer.h \
	path.cc \
	path.h \
	poll.h \
	poll_epoll.cc \
	poll_epoll.h \
	poll_kqueue.h \
	poll_kqueue.cc \
	poll_select.h \
	poll_select.cc \
	rate.cc \
	rate.h \
	torrent.cc \
	torrent.h \
	tracker.cc \
	tracker.h \
	tracker_list.cc \
	tracker_list.h

INCLUDES = -I$(srcdir) -I$(srcdir)/.. -I$(top_srcdir)

libtorrentincludedir = $(includedir)/torrent
libtorrentinclude_HEADERS = \
	connection_manager.h \
	download.h \
	exceptions.h \
	entry.h \
	event.h \
	http.h \
	object.h \
	object_stream.h \
	peer.h \
	path.h \
	poll.h \
	poll_epoll.h \
	poll_kqueue.h \
	poll_select.h \
	rate.h \
	torrent.h \
	tracker.h \
	tracker_list.h

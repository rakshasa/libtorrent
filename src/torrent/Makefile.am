SUBDIRS = \
	data \
	peer

noinst_LTLIBRARIES = libsub_torrent.la

libsub_torrent_la_SOURCES = \
	bitfield.cc \
	bitfield.h \
	chunk_manager.cc \
	chunk_manager.h \
	common.h \
	connection_manager.cc \
	connection_manager.h \
	dht_manager.cc \
	dht_manager.h \
	download.cc \
	download.h \
	error.cc \
	error.h \
	event.h \
	exceptions.cc \
	exceptions.h \
	hash_string.h \
	http.cc \
	http.h \
	object.cc \
	object.h \
	object_stream.cc \
	object_stream.h \
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
	resume.cc \
	resume.h \
	throttle.cc \
	throttle.h \
	torrent.cc \
	torrent.h \
	tracker.cc \
	tracker.h \
	tracker_list.cc \
	tracker_list.h

INCLUDES = -I$(srcdir) -I$(srcdir)/.. -I$(top_srcdir)

libtorrentincludedir = $(includedir)/torrent
libtorrentinclude_HEADERS = \
	bitfield.h \
	chunk_manager.h \
	common.h \
	connection_manager.h \
	dht_manager.h \
	download.h \
	error.h \
	exceptions.h \
	event.h \
	hash_string.h \
	http.h \
	object.h \
	object_stream.h \
	path.h \
	poll.h \
	poll_epoll.h \
	poll_kqueue.h \
	poll_select.h \
	rate.h \
	resume.h \
	throttle.h \
	torrent.h \
	tracker.h \
	tracker_list.h

noinst_LTLIBRARIES = libsub_torrentnet.la

libsub_torrentnet_la_SOURCES = \
	bind_manager.cc \
	bind_manager.h \
	socket_address_key.cc \
	socket_address_key.h

AM_CPPFLAGS = -I$(srcdir) -I$(srcdir)/.. -I$(srcdir)/../.. -I$(top_srcdir)

libtorrentincludedir = $(includedir)/torrent/net
libtorrentinclude_HEADERS = \
	bind_manager.h \
	socket_address_key.h

noinst_LTLIBRARIES = libsub_torrentnet.la

libsub_torrentnet_la_SOURCES = \
	address_info.cc \
	address_info.h \
	fd.cc \
	fd.h \
	socket_address.cc \
	socket_address.h \
	socket_address_key.cc \
	socket_address_key.h \
	socket_event.cc \
	socket_event.h \
	types.h

AM_CPPFLAGS = -I$(srcdir) -I$(srcdir)/.. -I$(srcdir)/../.. -I$(top_srcdir)

libtorrentincludedir = $(includedir)/torrent/net
libtorrentinclude_HEADERS = \
	address_info.h \
	fd.h \
	socket_address.h \
	socket_address_key.h \
	socket_event.h \
	types.h

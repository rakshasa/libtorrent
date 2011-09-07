noinst_LTLIBRARIES = libsub_torrentdownload.la

libsub_torrentdownload_la_SOURCES = \
	choke_group.cc \
	choke_group.h \
	choke_queue.cc \
	choke_queue.h \
	download_manager.cc \
	download_manager.h \
	group_entry.h \
	resource_manager.cc \
	resource_manager.h

INCLUDES = -I$(srcdir) -I$(srcdir)/.. -I$(srcdir)/../.. -I$(top_srcdir)

libtorrentincludedir = $(includedir)/torrent/download
libtorrentinclude_HEADERS = \
	choke_group.h \
	choke_queue.h \
	download_manager.h \
	group_entry.h \
	resource_manager.h

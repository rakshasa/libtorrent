noinst_LTLIBRARIES = libsub_torrentdata.la

libsub_torrentdata_la_SOURCES = \
	block.cc \
	block.h \
	block_failed.h \
	block_list.cc \
	block_list.h \
	block_transfer.h \
	file.cc \
	file.h \
	file_list.cc \
	file_list.h \
	piece.h \
	transfer_list.cc \
	transfer_list.h

INCLUDES = -I$(srcdir) -I$(srcdir)/.. -I$(srcdir)/../.. -I$(top_srcdir)

libtorrentincludedir = $(includedir)/torrent/data
libtorrentinclude_HEADERS = \
	block.h \
	block_list.h \
	block_transfer.h \
	file.h \
	file_list.h \
	piece.h \
	transfer_list.h

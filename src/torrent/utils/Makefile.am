noinst_LTLIBRARIES = libsub_torrentutils.la

libsub_torrentutils_la_SOURCES = \
	directory_events.cc \
	directory_events.h \
	extents.h \
	log.cc \
	log.h \
	log_buffer.cc \
	log_buffer.h \
	option_strings.cc \
	option_strings.h \
	random.cc \
	random.h \
	ranges.h \
	resume.cc \
	resume.h \
	signal_bitfield.cc \
	signal_bitfield.h \
	thread_base.cc \
	thread_base.h \
	thread_interrupt.cc \
	thread_interrupt.h \
	uri_parser.cc \
	uri_parser.h

AM_CPPFLAGS = -I$(srcdir) -I$(srcdir)/.. -I$(srcdir)/../.. -I$(top_srcdir)

libtorrentincludedir = $(includedir)/torrent/utils
libtorrentinclude_HEADERS = \
	directory_events.h \
	extents.h \
	log.h \
	log_buffer.h \
	option_strings.h \
	ranges.h \
	resume.h \
	signal_bitfield.h \
	thread_base.h \
	thread_interrupt.h \
	uri_parser.h

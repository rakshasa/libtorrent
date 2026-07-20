#include "config.h"

#include "test_fd.h"

#include <torrent/net/fd.h>

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(test_fd, "torrent/net");

void
test_fd::test_valid_flags() {
  CPPUNIT_ASSERT(torrent::fd_valid_flags(torrent::fd_flag_stream));
  CPPUNIT_ASSERT(torrent::fd_valid_flags(torrent::fd_flag_datagram));
  CPPUNIT_ASSERT(!torrent::fd_valid_flags(torrent::fd_flag_stream | torrent::fd_flag_datagram));

  CPPUNIT_ASSERT(torrent::fd_valid_flags(torrent::fd_flag_stream | torrent::fd_flag_nonblock));
  CPPUNIT_ASSERT(torrent::fd_valid_flags(torrent::fd_flag_stream | torrent::fd_flag_reuse_address));
  CPPUNIT_ASSERT(torrent::fd_valid_flags(torrent::fd_flag_stream | torrent::fd_flag_v4only));
  CPPUNIT_ASSERT(torrent::fd_valid_flags(torrent::fd_flag_stream | torrent::fd_flag_v6only));

  CPPUNIT_ASSERT(torrent::fd_valid_flags(torrent::fd_flag_datagram | torrent::fd_flag_nonblock));
  CPPUNIT_ASSERT(torrent::fd_valid_flags(torrent::fd_flag_datagram | torrent::fd_flag_reuse_address));
  CPPUNIT_ASSERT(torrent::fd_valid_flags(torrent::fd_flag_datagram | torrent::fd_flag_v4only));
  CPPUNIT_ASSERT(torrent::fd_valid_flags(torrent::fd_flag_datagram | torrent::fd_flag_v6only));

  CPPUNIT_ASSERT(!torrent::fd_valid_flags(torrent::fd_flag_v4only | torrent::fd_flag_v6only));
  CPPUNIT_ASSERT(!torrent::fd_valid_flags(torrent::fd_flag_stream | torrent::fd_flag_v4only | torrent::fd_flag_v6only));
  CPPUNIT_ASSERT(!torrent::fd_valid_flags(torrent::fd_flag_datagram | torrent::fd_flag_v4only | torrent::fd_flag_v6only));

  CPPUNIT_ASSERT(!torrent::fd_valid_flags(torrent::fd_flags()));
  CPPUNIT_ASSERT(!torrent::fd_valid_flags(torrent::fd_flags(~torrent::fd_flag_all)));
  CPPUNIT_ASSERT(!torrent::fd_valid_flags(torrent::fd_flags(torrent::fd_flag_stream | ~torrent::fd_flag_all)));
  CPPUNIT_ASSERT(!torrent::fd_valid_flags(torrent::fd_flags(0x3245132)));
}

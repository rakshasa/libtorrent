#include "config.h"

#include "test_socket_listen.h"

#include "helpers/network.h"
#include "net/socket_listen.h"

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(test_socket_listen, "net");

void
test_socket_listen::test_basic() {
  torrent::socket_listen sl;

  CPPUNIT_ASSERT(!sl.is_open());
  CPPUNIT_ASSERT(sl.backlog() == SOMAXCONN);
  CPPUNIT_ASSERT(sl.socket_address() == nullptr);
  CPPUNIT_ASSERT(sl.port() == 0);
  CPPUNIT_ASSERT(sl.type_name() == std::string("listen"));
}

void
test_socket_listen::test_open() {
  torrent::socket_listen sl;

  auto sin_any = wrap_ai_get_first_c_sa("0.0.0.0", "5050");
  // auto sin6_any = wrap_ai_get_first_sa("::");

  // auto sin_test = wrap_ai_get_first_sa("1.2.3.4", "5555");
  // auto sin6_test = wrap_ai_get_first_sa("ff01::1", "5555");
  // auto sin6_v4mapped_test = wrap_ai_get_first_sa("::ffff:1.2.3.4", "5555");

  mock_expect(&torrent::fd__socket, 1000, (int)PF_INET, (int)SOCK_STREAM, (int)IPPROTO_TCP);
  mock_expect(&torrent::fd__bind, 0, 1000, sin_any.get(), (socklen_t)sizeof(sockaddr_in));

  CPPUNIT_ASSERT(sl.open(torrent::sap_copy_addr(sin_any), 5000, 5099, 5050, torrent::fd_flags::fd_flag_stream | torrent::fd_flags::fd_flag_v4only));
  CPPUNIT_ASSERT(!sl.is_open());
  CPPUNIT_ASSERT(sl.port() == 5050);
  CPPUNIT_ASSERT(torrent::sa_equal(sl.socket_address(), sin_any.get()));

  CPPUNIT_ASSERT_NO_THROW(sl.close());
  CPPUNIT_ASSERT(!sl.is_open() && sl.socket_address() == nullptr && sl.port() == 0);

  CPPUNIT_ASSERT(sl.open(wrap_ai_get_first_sa("::"), 5000, 5099, 5050, torrent::fd_flags::fd_flag_stream));
  CPPUNIT_ASSERT(!sl.is_open());
  CPPUNIT_ASSERT(sl.port() == 5050);
  CPPUNIT_ASSERT(torrent::sa_equal(sl.socket_address(), wrap_ai_get_first_sa("::", "5050").get()));

  CPPUNIT_ASSERT_NO_THROW(sl.close());
  CPPUNIT_ASSERT(!sl.is_open() && sl.socket_address() == nullptr && sl.port() == 0);

  CPPUNIT_ASSERT(sl.open(wrap_ai_get_first_sa("1.2.3.4"), 5000, 5099, 5050, torrent::fd_flags::fd_flag_stream | torrent::fd_flags::fd_flag_v4only));
  CPPUNIT_ASSERT(!sl.is_open());
  CPPUNIT_ASSERT(sl.port() == 5050);
  CPPUNIT_ASSERT(torrent::sa_equal(sl.socket_address(), wrap_ai_get_first_sa("1.2.3.4", "5050").get()));

  CPPUNIT_ASSERT_NO_THROW(sl.close());
  CPPUNIT_ASSERT(!sl.is_open() && sl.socket_address() == nullptr && sl.port() == 0);

  CPPUNIT_ASSERT(sl.open(wrap_ai_get_first_sa("ff01::1"), 5000, 5099, 5050, torrent::fd_flags::fd_flag_stream | torrent::fd_flags::fd_flag_v6only));
  CPPUNIT_ASSERT(!sl.is_open());
  CPPUNIT_ASSERT(sl.port() == 5050);
  CPPUNIT_ASSERT(torrent::sa_equal(sl.socket_address(), wrap_ai_get_first_sa("ff01::1", "5050").get()));

  CPPUNIT_ASSERT_NO_THROW(sl.close());
  CPPUNIT_ASSERT(!sl.is_open() && sl.socket_address() == nullptr && sl.port() == 0);
}

// test stream, nonblock and reuse_address.

// TODO: errors on:
// sa with port address set
// not v4/v6
// default v4/v6
// invalid ports
// unknown flags
// fail on already open

// fd add randomize ports

// fd_flags open_flags = fd_flag_stream | fd_flag_nonblock | fd_flag_reuse_address;

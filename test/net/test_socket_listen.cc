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
  auto sin6_any = wrap_ai_get_first_c_sa("::", "5050");
  auto sin_1 = wrap_ai_get_first_c_sa("1.2.3.4", "5050");
  auto sin6_1 = wrap_ai_get_first_c_sa("ff01::1", "5050");

  mock_expect(&torrent::fd__socket, 1000, (int)PF_INET, (int)SOCK_STREAM, (int)IPPROTO_TCP);
  mock_expect(&torrent::fd__bind, 0, 1000, sin_any.get(), (socklen_t)sizeof(sockaddr_in));
  mock_expect(&torrent::fd__listen, 0, 1000, SOMAXCONN);
  CPPUNIT_ASSERT(sl.open(torrent::sap_copy_addr(sin_any), 5000, 5099, 5050, torrent::fd_flags::fd_flag_stream | torrent::fd_flags::fd_flag_v4only));
  CPPUNIT_ASSERT(sl.is_open());
  CPPUNIT_ASSERT(sl.port() == 5050);
  CPPUNIT_ASSERT(torrent::sa_equal(sl.socket_address(), sin_any.get()));

  mock_expect(&torrent::fd__close, 0, 1000);
  CPPUNIT_ASSERT_NO_THROW(sl.close());
  CPPUNIT_ASSERT(!sl.is_open() && sl.file_descriptor() == -1 && sl.socket_address() == nullptr && sl.port() == 0);

  mock_expect(&torrent::fd__socket, 1000, (int)PF_INET6, (int)SOCK_STREAM, (int)IPPROTO_TCP);
  mock_expect(&torrent::fd__bind, 0, 1000, sin6_any.get(), (socklen_t)sizeof(sockaddr_in6));
  mock_expect(&torrent::fd__listen, 0, 1000, SOMAXCONN);
  CPPUNIT_ASSERT(sl.open(torrent::sap_copy_addr(sin6_any), 5000, 5099, 5050, torrent::fd_flags::fd_flag_stream));
  CPPUNIT_ASSERT(sl.is_open());
  CPPUNIT_ASSERT(sl.port() == 5050);
  CPPUNIT_ASSERT(torrent::sa_equal(sl.socket_address(), sin6_any.get()));

  mock_expect(&torrent::fd__close, 0, 1000);
  CPPUNIT_ASSERT_NO_THROW(sl.close());
  CPPUNIT_ASSERT(!sl.is_open() && sl.file_descriptor() == -1 && sl.socket_address() == nullptr && sl.port() == 0);

  mock_expect(&torrent::fd__socket, 1000, (int)PF_INET, (int)SOCK_STREAM, (int)IPPROTO_TCP);
  mock_expect(&torrent::fd__bind, 0, 1000, sin_1.get(), (socklen_t)sizeof(sockaddr_in));
  mock_expect(&torrent::fd__listen, 0, 1000, SOMAXCONN);
  CPPUNIT_ASSERT(sl.open(torrent::sap_copy_addr(sin_1), 5000, 5099, 5050, torrent::fd_flags::fd_flag_stream | torrent::fd_flags::fd_flag_v4only));
  CPPUNIT_ASSERT(sl.is_open());
  CPPUNIT_ASSERT(sl.port() == 5050);
  CPPUNIT_ASSERT(torrent::sa_equal(sl.socket_address(), sin_1.get()));

  mock_expect(&torrent::fd__close, 0, 1000);
  CPPUNIT_ASSERT_NO_THROW(sl.close());
  CPPUNIT_ASSERT(!sl.is_open() && sl.file_descriptor() == -1 && sl.socket_address() == nullptr && sl.port() == 0);

  mock_expect(&torrent::fd__socket, 1000, (int)PF_INET6, (int)SOCK_STREAM, (int)IPPROTO_TCP);
  mock_expect(&torrent::fd__setsockopt_int, 0, 1000, (int)IPPROTO_IPV6, (int)IPV6_V6ONLY, (int)true);
  mock_expect(&torrent::fd__bind, 0, 1000, sin6_1.get(), (socklen_t)sizeof(sockaddr_in6));
  mock_expect(&torrent::fd__listen, 0, 1000, SOMAXCONN);
  CPPUNIT_ASSERT(sl.open(torrent::sap_copy_addr(sin6_1), 5000, 5099, 5050, torrent::fd_flags::fd_flag_stream | torrent::fd_flags::fd_flag_v6only));
  CPPUNIT_ASSERT(sl.is_open());
  CPPUNIT_ASSERT(sl.port() == 5050);
  CPPUNIT_ASSERT(torrent::sa_equal(sl.socket_address(), sin6_1.get()));

  mock_expect(&torrent::fd__close, 0, 1000);
  CPPUNIT_ASSERT_NO_THROW(sl.close());
  CPPUNIT_ASSERT(!sl.is_open() && sl.file_descriptor() == -1 && sl.socket_address() == nullptr && sl.port() == 0);
}

// test stream, nonblock and reuse_address.

// TODO: errors on:
// sa with port address set
// not v4/v6
// default v4/v6
// invalid ports
// unknown flags
// fail on already open
// fail on v4mapped

// fd add randomize ports

// fd_flags open_flags = fd_flag_stream | fd_flag_nonblock | fd_flag_reuse_address;

#include "config.h"

#include "test_socket_listen.h"

#include "helpers/fd.h"
#include "helpers/network.h"

#include <net/socket_listen.h>
#include <torrent/exceptions.h>
#include <torrent/utils/log.h>

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(test_socket_listen, "net");

#define TEST_SL_BEGIN(name)                                     \
  torrent::socket_listen sl;                                    \
  std::vector<torrent::sa_unique_ptr> sap_cache;                \
  lt_log_print(torrent::LOG_MOCK_CALLS, "sl_begin: %s", name);  \
  TEST_DEFAULT_SA;

#define TEST_SL_ASSERT_OPEN(_sap_bind, _sap_result, _flags)             \
  TEST_SL_ASSERT_OPEN_PORT(_sap_bind, _sap_result, 5000, 5009, 5005, _flags); \
  CPPUNIT_ASSERT(sl.port() == 5005);

#define TEST_SL_ASSERT_OPEN_PORT(_sap_bind, _sap_result, _first_port, _last_port, _itr_port, _flags) \
  CPPUNIT_ASSERT(sl.open(_sap_bind, _first_port, _last_port, _itr_port, _flags)); \
  CPPUNIT_ASSERT(sl.is_open());                                         \
  CPPUNIT_ASSERT(torrent::sa_equal(sl.socket_address(), _sap_result.get()));

#define TEST_SL_ASSERT_CLOSED()                          \
  CPPUNIT_ASSERT(!sl.is_open());                         \
  CPPUNIT_ASSERT(sl.file_descriptor() == -1);            \
  CPPUNIT_ASSERT(sl.socket_address() == nullptr);        \
  CPPUNIT_ASSERT(sl.port() == 0);

#define TEST_SL_CLOSE(_fd)                      \
  mock_expect(&torrent::fd__close, 0, _fd);     \
  CPPUNIT_ASSERT_NO_THROW(sl.close());          \
  TEST_SL_ASSERT_CLOSED();

#define TEST_SL_MOCK_CLOSED_PORT_RANGE(_src_sap, _first_port, _last_port) \
  { uint16_t _port = _first_port; do {                                  \
      sap_cache.push_back(torrent::sap_copy(_src_sap));                 \
      torrent::sap_set_port(sap_cache.back(), _port);                   \
      mock_expect(&torrent::fd__bind, -1, 1000,                         \
                  (const sockaddr*)sap_cache.back().get(),              \
                  (socklen_t)torrent::sap_length(sap_cache.back()));    \
    } while (_port++ != _last_port); }

void
test_socket_listen::test_basic() {
  TEST_SL_BEGIN("basic");
  TEST_SL_ASSERT_CLOSED();
  CPPUNIT_ASSERT(sl.backlog() == SOMAXCONN);
  CPPUNIT_ASSERT(sl.type_name() == std::string("listen"));
}

void
test_socket_listen::test_open_sap() {
  { TEST_SL_BEGIN("sin6_any, stream");
    mock_expect(&torrent::fd__socket, 1000, (int)PF_INET6, (int)SOCK_STREAM, (int)IPPROTO_TCP);
    mock_expect(&torrent::fd__bind, 0, 1000, c_sin6_any_5005.get(), (socklen_t)sizeof(sockaddr_in6));
    mock_expect(&torrent::fd__listen, 0, 1000, SOMAXCONN);
    TEST_SL_ASSERT_OPEN(torrent::sap_copy(sin6_any), c_sin6_any_5005, torrent::fd_flag_stream);
    TEST_SL_CLOSE(1000);
  };
  { TEST_SL_BEGIN("sin_any, stream|v4only");
    mock_expect(&torrent::fd__socket, 1000, (int)PF_INET, (int)SOCK_STREAM, (int)IPPROTO_TCP);
    mock_expect(&torrent::fd__bind, 0, 1000, c_sin_any_5005.get(), (socklen_t)sizeof(sockaddr_in));
    mock_expect(&torrent::fd__listen, 0, 1000, SOMAXCONN);
    TEST_SL_ASSERT_OPEN(torrent::sap_copy(sin_any), c_sin_any_5005, torrent::fd_flag_stream | torrent::fd_flag_v4only);
    TEST_SL_CLOSE(1000);
  };
  { TEST_SL_BEGIN("sin_1, stream|v4only");
    mock_expect(&torrent::fd__socket, 1000, (int)PF_INET, (int)SOCK_STREAM, (int)IPPROTO_TCP);
    mock_expect(&torrent::fd__bind, 0, 1000, c_sin_1_5005.get(), (socklen_t)sizeof(sockaddr_in));
    mock_expect(&torrent::fd__listen, 0, 1000, SOMAXCONN);
    TEST_SL_ASSERT_OPEN(torrent::sap_copy(sin_1), c_sin_1_5005, torrent::fd_flag_stream | torrent::fd_flag_v4only);
    TEST_SL_CLOSE(1000);
  };
  { TEST_SL_BEGIN("sin6_any, stream|v6only");
    mock_expect(&torrent::fd__socket, 1000, (int)PF_INET6, (int)SOCK_STREAM, (int)IPPROTO_TCP);
    mock_expect(&torrent::fd__setsockopt_int, 0, 1000, (int)IPPROTO_IPV6, (int)IPV6_V6ONLY, (int)true);
    mock_expect(&torrent::fd__bind, 0, 1000, c_sin6_any_5005.get(), (socklen_t)sizeof(sockaddr_in6));
    mock_expect(&torrent::fd__listen, 0, 1000, SOMAXCONN);
    TEST_SL_ASSERT_OPEN(torrent::sap_copy(sin6_any), c_sin6_any_5005, torrent::fd_flag_stream | torrent::fd_flag_v6only);
    TEST_SL_CLOSE(1000);
  };
  { TEST_SL_BEGIN("sin6_1, stream|v6only");
    mock_expect(&torrent::fd__socket, 1000, (int)PF_INET6, (int)SOCK_STREAM, (int)IPPROTO_TCP);
    mock_expect(&torrent::fd__setsockopt_int, 0, 1000, (int)IPPROTO_IPV6, (int)IPV6_V6ONLY, (int)true);
    mock_expect(&torrent::fd__bind, 0, 1000, c_sin6_1_5005.get(), (socklen_t)sizeof(sockaddr_in6));
    mock_expect(&torrent::fd__listen, 0, 1000, SOMAXCONN);
    TEST_SL_ASSERT_OPEN(torrent::sap_copy(sin6_1), c_sin6_1_5005, torrent::fd_flag_stream | torrent::fd_flag_v6only);
    TEST_SL_CLOSE(1000);
  };
}

void
test_socket_listen::test_open_flags() {
  { TEST_SL_BEGIN("sin_any, stream|v4only|nonblock");
    fd_expect_inet_tcp_nonblock(1000);
    mock_expect(&torrent::fd__bind, 0, 1000, c_sin_any_5005.get(), (socklen_t)sizeof(sockaddr_in));
    mock_expect(&torrent::fd__listen, 0, 1000, SOMAXCONN);
    TEST_SL_ASSERT_OPEN(torrent::sap_copy_addr(sin_any), c_sin_any_5005, torrent::fd_flag_stream | torrent::fd_flag_v4only | torrent::fd_flag_nonblock);
    TEST_SL_CLOSE(1000);
  };
  { TEST_SL_BEGIN("sin6_any, stream|nonblock");
    fd_expect_inet6_tcp_nonblock(1000);
    mock_expect(&torrent::fd__bind, 0, 1000, c_sin6_any_5005.get(), (socklen_t)sizeof(sockaddr_in6));
    mock_expect(&torrent::fd__listen, 0, 1000, SOMAXCONN);
    TEST_SL_ASSERT_OPEN(torrent::sap_copy(sin6_any), c_sin6_any_5005, torrent::fd_flag_stream | torrent::fd_flag_nonblock);
    TEST_SL_CLOSE(1000);
  };
  { TEST_SL_BEGIN("sin_any, stream|v4only|reuse_address");
    mock_expect(&torrent::fd__socket, 1000, (int)PF_INET, (int)SOCK_STREAM, (int)IPPROTO_TCP);
    mock_expect(&torrent::fd__setsockopt_int, 0, 1000, (int)SOL_SOCKET, (int)SO_REUSEADDR, (int)true);
    mock_expect(&torrent::fd__bind, 0, 1000, c_sin_any_5005.get(), (socklen_t)sizeof(sockaddr_in));
    mock_expect(&torrent::fd__listen, 0, 1000, SOMAXCONN);
    TEST_SL_ASSERT_OPEN(torrent::sap_copy_addr(sin_any), c_sin_any_5005, torrent::fd_flag_stream | torrent::fd_flag_v4only | torrent::fd_flag_reuse_address);
    TEST_SL_CLOSE(1000);
  };
  { TEST_SL_BEGIN("sin6_any, stream|reuse_address");
    mock_expect(&torrent::fd__socket, 1000, (int)PF_INET6, (int)SOCK_STREAM, (int)IPPROTO_TCP);
    mock_expect(&torrent::fd__setsockopt_int, 0, 1000, (int)SOL_SOCKET, (int)SO_REUSEADDR, (int)true);
    mock_expect(&torrent::fd__bind, 0, 1000, c_sin6_any_5005.get(), (socklen_t)sizeof(sockaddr_in6));
    mock_expect(&torrent::fd__listen, 0, 1000, SOMAXCONN);
    TEST_SL_ASSERT_OPEN(torrent::sap_copy(sin6_any), c_sin6_any_5005, torrent::fd_flag_stream | torrent::fd_flag_reuse_address);
    TEST_SL_CLOSE(1000);
  };
  { TEST_SL_BEGIN("sin_any, stream|v4only|nonblock|reuse_address");
    fd_expect_inet_tcp_nonblock(1000);
    mock_expect(&torrent::fd__setsockopt_int, 0, 1000, (int)SOL_SOCKET, (int)SO_REUSEADDR, (int)true);
    mock_expect(&torrent::fd__bind, 0, 1000, c_sin_any_5005.get(), (socklen_t)sizeof(sockaddr_in));
    mock_expect(&torrent::fd__listen, 0, 1000, SOMAXCONN);
    TEST_SL_ASSERT_OPEN(torrent::sap_copy_addr(sin_any), c_sin_any_5005, torrent::fd_flag_stream | torrent::fd_flag_v4only | torrent::fd_flag_nonblock | torrent::fd_flag_reuse_address);
    TEST_SL_CLOSE(1000);
  };
  { TEST_SL_BEGIN("sin6_any, stream|nonblock|reuse_address");
    fd_expect_inet6_tcp_nonblock_reuseaddr(1000);
    mock_expect(&torrent::fd__bind, 0, 1000, c_sin6_any_5005.get(), (socklen_t)sizeof(sockaddr_in6));
    mock_expect(&torrent::fd__listen, 0, 1000, SOMAXCONN);
    TEST_SL_ASSERT_OPEN(torrent::sap_copy(sin6_any), c_sin6_any_5005, torrent::fd_flag_stream | torrent::fd_flag_nonblock | torrent::fd_flag_reuse_address);
    TEST_SL_CLOSE(1000);
  };
}

void
test_socket_listen::test_open_port_single() {
  { TEST_SL_BEGIN("sin6_any, stream");
    mock_expect(&torrent::fd__socket, 1000, (int)PF_INET6, (int)SOCK_STREAM, (int)IPPROTO_TCP);
    mock_expect(&torrent::fd__bind, 0, 1000, c_sin6_any_5000.get(), (socklen_t)sizeof(sockaddr_in6));
    mock_expect(&torrent::fd__listen, 0, 1000, SOMAXCONN);
    TEST_SL_ASSERT_OPEN_PORT(torrent::sap_copy(sin6_any), c_sin6_any_5000, 5000, 5000, 5000, torrent::fd_flag_stream);
    TEST_SL_CLOSE(1000);
  };
}

void
test_socket_listen::test_open_port_range() {
  { TEST_SL_BEGIN("sin6_any, stream, first");
    mock_expect(&torrent::fd__socket, 1000, (int)PF_INET6, (int)SOCK_STREAM, (int)IPPROTO_TCP);
    mock_expect(&torrent::fd__bind, 0, 1000, c_sin6_any_5000.get(), (socklen_t)sizeof(sockaddr_in6));
    mock_expect(&torrent::fd__listen, 0, 1000, SOMAXCONN);
    TEST_SL_ASSERT_OPEN_PORT(torrent::sap_copy(sin6_any), c_sin6_any_5000, 5000, 5010, 5000, torrent::fd_flag_stream);
    TEST_SL_CLOSE(1000);
  };
  { TEST_SL_BEGIN("sin6_any, stream, from first to middle port");
    mock_expect(&torrent::fd__socket, 1000, (int)PF_INET6, (int)SOCK_STREAM, (int)IPPROTO_TCP);
    TEST_SL_MOCK_CLOSED_PORT_RANGE(sin6_any, 5000, 5004);
    mock_expect(&torrent::fd__bind, 0, 1000, c_sin6_any_5005.get(), (socklen_t)sizeof(sockaddr_in6));
    mock_expect(&torrent::fd__listen, 0, 1000, SOMAXCONN);
    TEST_SL_ASSERT_OPEN_PORT(torrent::sap_copy(sin6_any), c_sin6_any_5005, 5000, 5010, 5000, torrent::fd_flag_stream);
    TEST_SL_CLOSE(1000);
  };
  { TEST_SL_BEGIN("sin6_any, stream, from first to last port");
    mock_expect(&torrent::fd__socket, 1000, (int)PF_INET6, (int)SOCK_STREAM, (int)IPPROTO_TCP);
    TEST_SL_MOCK_CLOSED_PORT_RANGE(sin6_any, 5000, 5009);
    mock_expect(&torrent::fd__bind, 0, 1000, c_sin6_any_5010.get(), (socklen_t)sizeof(sockaddr_in6));
    mock_expect(&torrent::fd__listen, 0, 1000, SOMAXCONN);
    TEST_SL_ASSERT_OPEN_PORT(torrent::sap_copy(sin6_any), c_sin6_any_5010, 5000, 5010, 5000, torrent::fd_flag_stream);
    TEST_SL_CLOSE(1000);
  };
  { TEST_SL_BEGIN("sin6_any, stream, middle");
    mock_expect(&torrent::fd__socket, 1000, (int)PF_INET6, (int)SOCK_STREAM, (int)IPPROTO_TCP);
    mock_expect(&torrent::fd__bind, 0, 1000, c_sin6_any_5005.get(), (socklen_t)sizeof(sockaddr_in6));
    mock_expect(&torrent::fd__listen, 0, 1000, SOMAXCONN);
    TEST_SL_ASSERT_OPEN_PORT(torrent::sap_copy(sin6_any), c_sin6_any_5005, 5000, 5010, 5005, torrent::fd_flag_stream);
    TEST_SL_CLOSE(1000);
  };
  { TEST_SL_BEGIN("sin6_any, stream, from middle to last port");
    mock_expect(&torrent::fd__socket, 1000, (int)PF_INET6, (int)SOCK_STREAM, (int)IPPROTO_TCP);
    TEST_SL_MOCK_CLOSED_PORT_RANGE(sin6_any, 5005, 5009);
    mock_expect(&torrent::fd__bind, 0, 1000, c_sin6_any_5010.get(), (socklen_t)sizeof(sockaddr_in6));
    mock_expect(&torrent::fd__listen, 0, 1000, SOMAXCONN);
    TEST_SL_ASSERT_OPEN_PORT(torrent::sap_copy(sin6_any), c_sin6_any_5010, 5000, 5010, 5005, torrent::fd_flag_stream);
    TEST_SL_CLOSE(1000);
  };
  { TEST_SL_BEGIN("sin6_any, stream, last");
    mock_expect(&torrent::fd__socket, 1000, (int)PF_INET6, (int)SOCK_STREAM, (int)IPPROTO_TCP);
    mock_expect(&torrent::fd__bind, 0, 1000, c_sin6_any_5010.get(), (socklen_t)sizeof(sockaddr_in6));
    mock_expect(&torrent::fd__listen, 0, 1000, SOMAXCONN);
    TEST_SL_ASSERT_OPEN_PORT(torrent::sap_copy(sin6_any), c_sin6_any_5010, 5000, 5010, 5010, torrent::fd_flag_stream);
    TEST_SL_CLOSE(1000);
  };
  { TEST_SL_BEGIN("sin6_any, stream, from last to first port");
    mock_expect(&torrent::fd__socket, 1000, (int)PF_INET6, (int)SOCK_STREAM, (int)IPPROTO_TCP);
    TEST_SL_MOCK_CLOSED_PORT_RANGE(sin6_any, 5010, 5010);
    mock_expect(&torrent::fd__bind, 0, 1000, c_sin6_any_5000.get(), (socklen_t)sizeof(sockaddr_in6));
    mock_expect(&torrent::fd__listen, 0, 1000, SOMAXCONN);
    TEST_SL_ASSERT_OPEN_PORT(torrent::sap_copy(sin6_any), c_sin6_any_5000, 5000, 5010, 5010, torrent::fd_flag_stream);
    TEST_SL_CLOSE(1000);
  };
  { TEST_SL_BEGIN("sin6_any, stream, from last to middle port");
    mock_expect(&torrent::fd__socket, 1000, (int)PF_INET6, (int)SOCK_STREAM, (int)IPPROTO_TCP);
    TEST_SL_MOCK_CLOSED_PORT_RANGE(sin6_any, 5010, 5010);
    TEST_SL_MOCK_CLOSED_PORT_RANGE(sin6_any, 5000, 5004);
    mock_expect(&torrent::fd__bind, 0, 1000, c_sin6_any_5005.get(), (socklen_t)sizeof(sockaddr_in6));
    mock_expect(&torrent::fd__listen, 0, 1000, SOMAXCONN);
    TEST_SL_ASSERT_OPEN_PORT(torrent::sap_copy(sin6_any), c_sin6_any_5005, 5000, 5010, 5010, torrent::fd_flag_stream);
    TEST_SL_CLOSE(1000);
  };
}

void
test_socket_listen::test_open_error() {
  { TEST_SL_BEGIN("open twice");
    fd_expect_inet6_tcp_nonblock(1000);
    mock_expect(&torrent::fd__bind, 0, 1000, c_sin6_any_5005.get(), (socklen_t)sizeof(sockaddr_in6));
    mock_expect(&torrent::fd__listen, 0, 1000, SOMAXCONN);
    TEST_SL_ASSERT_OPEN(torrent::sap_copy(sin6_any), c_sin6_any_5005, torrent::fd_flag_stream | torrent::fd_flag_nonblock);
    CPPUNIT_ASSERT_THROW(sl.open(torrent::sap_copy(sin6_any), 5000, 5009, 5005, torrent::fd_flag_stream),
                         torrent::internal_error);
    TEST_SL_CLOSE(1000);
  };
  { TEST_SL_BEGIN("sin_any, stream, no v4only");
    CPPUNIT_ASSERT_THROW(sl.open(torrent::sap_copy(sin_any), 5000, 5009, 5005, torrent::fd_flag_stream),
                         torrent::internal_error);
    TEST_SL_ASSERT_CLOSED();
  };
}

void
test_socket_listen::test_open_error_sap() {
  { TEST_SL_BEGIN("unspec");
    CPPUNIT_ASSERT_THROW(sl.open(torrent::sa_make_unspec(), 5000, 5009, 5005, torrent::fd_flag_stream), torrent::internal_error);
    TEST_SL_ASSERT_CLOSED();
  };
  { TEST_SL_BEGIN("unix");
    CPPUNIT_ASSERT_THROW(sl.open(torrent::sa_make_unix("test"), 5000, 5009, 5005, torrent::fd_flag_stream), torrent::internal_error);
    TEST_SL_ASSERT_CLOSED();
  };
  { TEST_SL_BEGIN("sin_any_5005");
    CPPUNIT_ASSERT_THROW(sl.open(torrent::sap_copy(sin_any_5005), 5000, 5009, 5005, torrent::fd_flag_stream | torrent::fd_flag_v4only), torrent::internal_error);
    TEST_SL_ASSERT_CLOSED();
  };
  { TEST_SL_BEGIN("sin6_any_5005");
    CPPUNIT_ASSERT_THROW(sl.open(torrent::sap_copy(sin6_any_5005), 5000, 5009, 5005, torrent::fd_flag_stream), torrent::internal_error);
    TEST_SL_ASSERT_CLOSED();
  };
  { TEST_SL_BEGIN("sin_any");
    CPPUNIT_ASSERT_THROW(sl.open(torrent::sap_to_v4mapped(sin_any), 5000, 5009, 5005, torrent::fd_flag_stream | torrent::fd_flag_v4only), torrent::internal_error);
    TEST_SL_ASSERT_CLOSED();
  };
  { TEST_SL_BEGIN("sin_1, v4mapped");
    CPPUNIT_ASSERT_THROW(sl.open(torrent::sap_to_v4mapped(sin_1), 5000, 5009, 5005, torrent::fd_flag_stream | torrent::fd_flag_v4only), torrent::internal_error);
    TEST_SL_ASSERT_CLOSED();
  };
  { TEST_SL_BEGIN("sin_broadcast");
    CPPUNIT_ASSERT_THROW(sl.open(torrent::sap_copy(sin_bc), 5000, 5009, 5005, torrent::fd_flag_stream | torrent::fd_flag_v4only), torrent::internal_error);
    TEST_SL_ASSERT_CLOSED();
  };
}

void
test_socket_listen::test_open_error_flags() {
  { TEST_SL_BEGIN("sin6_any, fd_flags(0)");
    CPPUNIT_ASSERT_THROW(sl.open(torrent::sap_copy(sin6_any), 5000, 5009, 5005, torrent::fd_flags(0)), torrent::internal_error);
    TEST_SL_ASSERT_CLOSED();
  };
  { TEST_SL_BEGIN("sin6_any, fd_flags(0xffff)");
    CPPUNIT_ASSERT_THROW(sl.open(torrent::sap_copy(sin6_any), 5000, 5009, 5005, torrent::fd_flags(0xffff)), torrent::internal_error);
    TEST_SL_ASSERT_CLOSED();
  };
}

void
test_socket_listen::test_open_error_port_single() {
  { TEST_SL_BEGIN("sin6_any, 0, 0, 0");
    CPPUNIT_ASSERT_THROW(sl.open(torrent::sap_copy(sin6_any), 0, 0, 0, torrent::fd_flag_stream), torrent::internal_error);
    TEST_SL_ASSERT_CLOSED();
  };
  { TEST_SL_BEGIN("sin6_any, 1000, 0, 0");
    CPPUNIT_ASSERT_THROW(sl.open(torrent::sap_copy(sin6_any), 1000, 0, 0, torrent::fd_flag_stream), torrent::internal_error);
    TEST_SL_ASSERT_CLOSED();
  };
  { TEST_SL_BEGIN("sin6_any, 0, 1000, 0");
    CPPUNIT_ASSERT_THROW(sl.open(torrent::sap_copy(sin6_any), 0, 1000, 0, torrent::fd_flag_stream), torrent::internal_error);
    TEST_SL_ASSERT_CLOSED();
  };
  { TEST_SL_BEGIN("sin6_any, 0, 0, 500");
    CPPUNIT_ASSERT_THROW(sl.open(torrent::sap_copy(sin6_any), 0, 0, 500, torrent::fd_flag_stream), torrent::internal_error);
    TEST_SL_ASSERT_CLOSED();
  };
  { TEST_SL_BEGIN("sin6_any, 0, 1000, 500");
    CPPUNIT_ASSERT_THROW(sl.open(torrent::sap_copy(sin6_any), 0, 1000, 500, torrent::fd_flag_stream), torrent::internal_error);
    TEST_SL_ASSERT_CLOSED();
  };
}

void
test_socket_listen::test_open_error_port_range() {
  { TEST_SL_BEGIN("sin6_any, first > last");
    CPPUNIT_ASSERT_THROW(sl.open(torrent::sap_copy(sin6_any), 5000, 4999, 5000, torrent::fd_flag_stream), torrent::internal_error);
    TEST_SL_ASSERT_CLOSED();
  };
  { TEST_SL_BEGIN("sin6_any, first > itr");
    CPPUNIT_ASSERT_THROW(sl.open(torrent::sap_copy(sin6_any), 5001, 5009, 5000, torrent::fd_flag_stream), torrent::internal_error);
    TEST_SL_ASSERT_CLOSED();
  };
  { TEST_SL_BEGIN("sin6_any, itr > last");
    CPPUNIT_ASSERT_THROW(sl.open(torrent::sap_copy(sin6_any), 5000, 5009, 5010, torrent::fd_flag_stream), torrent::internal_error);
    TEST_SL_ASSERT_CLOSED();
  };
  { TEST_SL_BEGIN("sin6_any, min first > last");
    CPPUNIT_ASSERT_THROW(sl.open(torrent::sap_copy(sin6_any), 2, 1, 2, torrent::fd_flag_stream), torrent::internal_error);
    TEST_SL_ASSERT_CLOSED();
  };
  { TEST_SL_BEGIN("sin6_any, min first > itr");
    CPPUNIT_ASSERT_THROW(sl.open(torrent::sap_copy(sin6_any), 2, 1000, 1, torrent::fd_flag_stream), torrent::internal_error);
    TEST_SL_ASSERT_CLOSED();
  };
  { TEST_SL_BEGIN("sin6_any, min itr > last");
    CPPUNIT_ASSERT_THROW(sl.open(torrent::sap_copy(sin6_any), 1, 2, 3, torrent::fd_flag_stream), torrent::internal_error);
    TEST_SL_ASSERT_CLOSED();
  };
  { TEST_SL_BEGIN("sin6_any, max first > last");
    CPPUNIT_ASSERT_THROW(sl.open(torrent::sap_copy(sin6_any), 0xffff, 0xfffe, 0xffff, torrent::fd_flag_stream), torrent::internal_error);
    TEST_SL_ASSERT_CLOSED();
  };
  { TEST_SL_BEGIN("sin6_any, max first > itr");
    CPPUNIT_ASSERT_THROW(sl.open(torrent::sap_copy(sin6_any), 0xffff, 0xffff, 0xfffe, torrent::fd_flag_stream), torrent::internal_error);
    TEST_SL_ASSERT_CLOSED();
  };
  { TEST_SL_BEGIN("sin6_any, max itr > last");
    CPPUNIT_ASSERT_THROW(sl.open(torrent::sap_copy(sin6_any), 0xfffe, 0xfffe, 0xffff, torrent::fd_flag_stream), torrent::internal_error);
    TEST_SL_ASSERT_CLOSED();
  };
}

// deal with reuse error
// fd add randomize ports

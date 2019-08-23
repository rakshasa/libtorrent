#include "config.h"

#include "test_socket_listen.h"

#include "helpers/expect_fd.h"
#include "helpers/expect_utils.h"
#include "helpers/mock_function.h"
#include "helpers/network.h"

#include <net/socket_listen.h>
#include <torrent/exceptions.h>
#include <torrent/utils/log.h>

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(test_socket_listen, "net");

struct test_sl_deleter {
  void operator()(torrent::socket_listen* sl) const { if (!sl->is_open()) delete sl; }
};

typedef std::unique_ptr<torrent::socket_listen, test_sl_deleter> test_sl_unique_ptr;

#define TEST_SL_BEGIN(name)                                     \
  test_sl_unique_ptr sl(new torrent::socket_listen);            \
  std::vector<torrent::sa_unique_ptr> sap_cache;                \
  lt_log_print(torrent::LOG_MOCK_CALLS, "sl_begin: %s", name);  \
  TEST_DEFAULT_SA;

#define TEST_SL_ASSERT_OPEN(_sap_bind, _sap_result, _flags)             \
  TEST_SL_ASSERT_OPEN_PORT(_sap_bind, _sap_result, 5000, 5009, 5005, _flags); \
  CPPUNIT_ASSERT(sl->socket_address_port() == 5005);

#define TEST_SL_ASSERT_OPEN_PORT(_sap_bind, _sap_result, _first_port, _last_port, _itr_port, _flags) \
  expect_event_open_re(0);                                              \
  CPPUNIT_ASSERT(sl->open(_sap_bind, _first_port, _last_port, _itr_port, _flags)); \
  CPPUNIT_ASSERT(sl->is_open());                                        \
  CPPUNIT_ASSERT(torrent::sa_equal(sl->socket_address(), _sap_result.get()));

#define TEST_SL_ASSERT_OPEN_SEQUENTIAL(_sap_bind, _sap_result, _first_port, _last_port, _flags) \
  expect_event_open_re(0);                                              \
  CPPUNIT_ASSERT(sl->open_sequential(_sap_bind, _first_port, _last_port, _flags)); \
  CPPUNIT_ASSERT(sl->is_open());                                        \
  CPPUNIT_ASSERT(torrent::sa_equal(sl->socket_address(), _sap_result.get()));

#define TEST_SL_ASSERT_OPEN_RANDOMIZE(_sap_bind, _sap_result, _first_port, _last_port, _flags) \
  expect_event_open_re(0);                                              \
  CPPUNIT_ASSERT(sl->open_randomize(_sap_bind, _first_port, _last_port, _flags)); \
  CPPUNIT_ASSERT(sl->is_open());                                        \
  CPPUNIT_ASSERT(torrent::sa_equal(sl->socket_address(), _sap_result.get()));

#define TEST_SL_ASSERT_CLOSED()                           \
  CPPUNIT_ASSERT(!sl->is_open());                         \
  CPPUNIT_ASSERT(sl->file_descriptor() == -1);            \
  CPPUNIT_ASSERT(sl->socket_address() == nullptr);        \
  CPPUNIT_ASSERT(sl->socket_address_port() == 0);

#define TEST_SL_CLOSE(_fd)                                              \
  mock_expect(&torrent::fd__close, 0, _fd);                             \
  mock_expect(&torrent::poll_event_closed, (torrent::Event*)sl.get());  \
  CPPUNIT_ASSERT_NO_THROW(sl->close());                                 \
  TEST_SL_ASSERT_CLOSED();

#define TEST_SL_MOCK_CLOSED_PORT_RANGE(_src_sap, _first_port, _last_port) \
  { uint16_t _port = _first_port; do {                                  \
      sap_cache.push_back(torrent::sap_copy(_src_sap));                 \
      torrent::sap_set_port(sap_cache.back(), _port);                   \
      mock_expect(&torrent::fd__bind, -1, 1000,                         \
                  (const sockaddr*)sap_cache.back().get(),              \
                  (socklen_t)torrent::sap_length(sap_cache.back()));    \
    } while (_port++ != _last_port);                                    \
  }

void
test_socket_listen::test_basic() {
  TEST_SL_BEGIN("basic");
  TEST_SL_ASSERT_CLOSED();
  CPPUNIT_ASSERT(sl->backlog() == SOMAXCONN);
  CPPUNIT_ASSERT(sl->type_name() == std::string("socket_listen"));
}

void
test_socket_listen::test_open_error() {
  { TEST_SL_BEGIN("open twice");
    expect_fd_inet6_tcp_nonblock(1000);
    expect_fd_bind_listen(1000, c_sin6_any_5005);
    TEST_SL_ASSERT_OPEN(torrent::sap_copy(sin6_any), c_sin6_any_5005, torrent::fd_flag_stream | torrent::fd_flag_nonblock);
    CPPUNIT_ASSERT_THROW(sl->open(torrent::sap_copy(sin6_any), 5000, 5009, 5005, torrent::fd_flag_stream),
                         torrent::internal_error);
    TEST_SL_CLOSE(1000);
  };
  { TEST_SL_BEGIN("sin_any, stream, no v4only");
    CPPUNIT_ASSERT_THROW(sl->open(torrent::sap_copy(sin_any), 5000, 5009, 5005, torrent::fd_flag_stream),
                         torrent::internal_error);
    TEST_SL_ASSERT_CLOSED();
  };
}

void
test_socket_listen::test_open_sap() {
  { TEST_SL_BEGIN("sin6_any, stream");
    expect_fd_inet6_tcp(1000);
    expect_fd_bind_listen(1000, c_sin6_any_5005);
    TEST_SL_ASSERT_OPEN(torrent::sap_copy(sin6_any), c_sin6_any_5005, torrent::fd_flag_stream);
    TEST_SL_CLOSE(1000);
  };
  { TEST_SL_BEGIN("sin_any, stream|v4only");
    expect_fd_inet_tcp(1000);
    expect_fd_bind_listen(1000, c_sin_any_5005);
    TEST_SL_ASSERT_OPEN(torrent::sap_copy(sin_any), c_sin_any_5005, torrent::fd_flag_stream | torrent::fd_flag_v4only);
    TEST_SL_CLOSE(1000);
  };
  { TEST_SL_BEGIN("sin_1, stream|v4only");
    expect_fd_inet_tcp(1000);
    expect_fd_bind_listen(1000, c_sin_1_5005);
    TEST_SL_ASSERT_OPEN(torrent::sap_copy(sin_1), c_sin_1_5005, torrent::fd_flag_stream | torrent::fd_flag_v4only);
    TEST_SL_CLOSE(1000);
  };
  { TEST_SL_BEGIN("sin6_any, stream|v6only");
    expect_fd_inet6_tcp(1000);
    mock_expect(&torrent::fd__setsockopt_int, 0, 1000, (int)IPPROTO_IPV6, (int)IPV6_V6ONLY, (int)true);
    expect_fd_bind_listen(1000, c_sin6_any_5005);
    TEST_SL_ASSERT_OPEN(torrent::sap_copy(sin6_any), c_sin6_any_5005, torrent::fd_flag_stream | torrent::fd_flag_v6only);
    TEST_SL_CLOSE(1000);
  };
  { TEST_SL_BEGIN("sin6_1, stream|v6only");
    expect_fd_inet6_tcp(1000);
    mock_expect(&torrent::fd__setsockopt_int, 0, 1000, (int)IPPROTO_IPV6, (int)IPV6_V6ONLY, (int)true);
    expect_fd_bind_listen(1000, c_sin6_1_5005);
    TEST_SL_ASSERT_OPEN(torrent::sap_copy(sin6_1), c_sin6_1_5005, torrent::fd_flag_stream | torrent::fd_flag_v6only);
    TEST_SL_CLOSE(1000);
  };
}

void
test_socket_listen::test_open_sap_error() {
  { TEST_SL_BEGIN("unspec");
    CPPUNIT_ASSERT_THROW(sl->open(torrent::sa_make_unspec(), 5000, 5009, 5005, torrent::fd_flag_stream), torrent::internal_error);
    TEST_SL_ASSERT_CLOSED();
  };
  { TEST_SL_BEGIN("unix");
    CPPUNIT_ASSERT_THROW(sl->open(torrent::sa_make_unix("test"), 5000, 5009, 5005, torrent::fd_flag_stream), torrent::internal_error);
    TEST_SL_ASSERT_CLOSED();
  };
  { TEST_SL_BEGIN("sin_any_5005");
    CPPUNIT_ASSERT_THROW(sl->open(torrent::sap_copy(sin_any_5005), 5000, 5009, 5005, torrent::fd_flag_stream | torrent::fd_flag_v4only), torrent::internal_error);
    TEST_SL_ASSERT_CLOSED();
  };
  { TEST_SL_BEGIN("sin6_any_5005");
    CPPUNIT_ASSERT_THROW(sl->open(torrent::sap_copy(sin6_any_5005), 5000, 5009, 5005, torrent::fd_flag_stream), torrent::internal_error);
    TEST_SL_ASSERT_CLOSED();
  };
  { TEST_SL_BEGIN("sin_any");
    CPPUNIT_ASSERT_THROW(sl->open(torrent::sap_to_v4mapped(sin_any), 5000, 5009, 5005, torrent::fd_flag_stream | torrent::fd_flag_v4only), torrent::internal_error);
    TEST_SL_ASSERT_CLOSED();
  };
  { TEST_SL_BEGIN("sin_1, v4mapped");
    CPPUNIT_ASSERT_THROW(sl->open(torrent::sap_to_v4mapped(sin_1), 5000, 5009, 5005, torrent::fd_flag_stream | torrent::fd_flag_v4only), torrent::internal_error);
    TEST_SL_ASSERT_CLOSED();
  };
  { TEST_SL_BEGIN("sin_broadcast");
    CPPUNIT_ASSERT_THROW(sl->open(torrent::sap_copy(sin_bc), 5000, 5009, 5005, torrent::fd_flag_stream | torrent::fd_flag_v4only), torrent::internal_error);
    TEST_SL_ASSERT_CLOSED();
  };
}

void
test_socket_listen::test_open_flags() {
  { TEST_SL_BEGIN("sin_any, stream|v4only|nonblock");
    expect_fd_inet_tcp_nonblock(1000);
    expect_fd_bind_listen(1000, c_sin_any_5005);
    TEST_SL_ASSERT_OPEN(torrent::sap_copy_addr(sin_any), c_sin_any_5005, torrent::fd_flag_stream | torrent::fd_flag_v4only | torrent::fd_flag_nonblock);
    TEST_SL_CLOSE(1000);
  };
  { TEST_SL_BEGIN("sin6_any, stream|nonblock");
    expect_fd_inet6_tcp_nonblock(1000);
    expect_fd_bind_listen(1000, c_sin6_any_5005);
    TEST_SL_ASSERT_OPEN(torrent::sap_copy(sin6_any), c_sin6_any_5005, torrent::fd_flag_stream | torrent::fd_flag_nonblock);
    TEST_SL_CLOSE(1000);
  };
  { TEST_SL_BEGIN("sin_any, stream|v4only|reuse_address");
    expect_fd_inet_tcp(1000);
    mock_expect(&torrent::fd__setsockopt_int, 0, 1000, (int)SOL_SOCKET, (int)SO_REUSEADDR, (int)true);
    expect_fd_bind_listen(1000, c_sin_any_5005);
    TEST_SL_ASSERT_OPEN(torrent::sap_copy_addr(sin_any), c_sin_any_5005, torrent::fd_flag_stream | torrent::fd_flag_v4only | torrent::fd_flag_reuse_address);
    TEST_SL_CLOSE(1000);
  };
  { TEST_SL_BEGIN("sin6_any, stream|reuse_address");
    expect_fd_inet6_tcp(1000);
    mock_expect(&torrent::fd__setsockopt_int, 0, 1000, (int)SOL_SOCKET, (int)SO_REUSEADDR, (int)true);
    expect_fd_bind_listen(1000, c_sin6_any_5005);
    TEST_SL_ASSERT_OPEN(torrent::sap_copy(sin6_any), c_sin6_any_5005, torrent::fd_flag_stream | torrent::fd_flag_reuse_address);
    TEST_SL_CLOSE(1000);
  };
  { TEST_SL_BEGIN("sin_any, stream|v4only|nonblock|reuse_address");
    expect_fd_inet_tcp_nonblock(1000);
    mock_expect(&torrent::fd__setsockopt_int, 0, 1000, (int)SOL_SOCKET, (int)SO_REUSEADDR, (int)true);
    expect_fd_bind_listen(1000, c_sin_any_5005);
    TEST_SL_ASSERT_OPEN(torrent::sap_copy_addr(sin_any), c_sin_any_5005, torrent::fd_flag_stream | torrent::fd_flag_v4only | torrent::fd_flag_nonblock | torrent::fd_flag_reuse_address);
    TEST_SL_CLOSE(1000);
  };
  { TEST_SL_BEGIN("sin6_any, stream|nonblock|reuse_address");
    expect_fd_inet6_tcp_nonblock_reuseaddr(1000);
    expect_fd_bind_listen(1000, c_sin6_any_5005);
    TEST_SL_ASSERT_OPEN(torrent::sap_copy(sin6_any), c_sin6_any_5005, torrent::fd_flag_stream | torrent::fd_flag_nonblock | torrent::fd_flag_reuse_address);
    TEST_SL_CLOSE(1000);
  };
}

void
test_socket_listen::test_open_flags_error() {
  { TEST_SL_BEGIN("sin6_any, fd_flags(0)");
    CPPUNIT_ASSERT_THROW(sl->open(torrent::sap_copy(sin6_any), 5000, 5009, 5005, torrent::fd_flags(0)), torrent::internal_error);
    TEST_SL_ASSERT_CLOSED();
  };
  { TEST_SL_BEGIN("sin6_any, fd_flags(0xffff)");
    CPPUNIT_ASSERT_THROW(sl->open(torrent::sap_copy(sin6_any), 5000, 5009, 5005, torrent::fd_flags(0xffff)), torrent::internal_error);
    TEST_SL_ASSERT_CLOSED();
  };
}

void
test_socket_listen::test_open_port_single() {
  { TEST_SL_BEGIN("sin6_any, stream");
    expect_fd_inet6_tcp(1000);
    expect_fd_bind_listen(1000, c_sin6_any_5000);
    TEST_SL_ASSERT_OPEN_PORT(torrent::sap_copy(sin6_any), c_sin6_any_5000, 5000, 5000, 5000, torrent::fd_flag_stream);
    TEST_SL_CLOSE(1000);
  };
  { TEST_SL_BEGIN("sin_any, stream");
    expect_fd_inet_tcp(1000);
    expect_fd_bind_listen(1000, c_sin_any_5000);
    TEST_SL_ASSERT_OPEN_PORT(torrent::sap_copy(sin_any), c_sin_any_5000, 5000, 5000, 5000, torrent::fd_flag_stream | torrent::fd_flag_v4only);
    TEST_SL_CLOSE(1000);
  };
}

void
test_socket_listen::test_open_port_single_error() {
  { TEST_SL_BEGIN("sin6_any, 0, 0, 0");
    CPPUNIT_ASSERT_THROW(sl->open(torrent::sap_copy(sin6_any), 0, 0, 0, torrent::fd_flag_stream), torrent::internal_error);
    TEST_SL_ASSERT_CLOSED();
  };
  { TEST_SL_BEGIN("sin6_any, 1000, 0, 0");
    CPPUNIT_ASSERT_THROW(sl->open(torrent::sap_copy(sin6_any), 1000, 0, 0, torrent::fd_flag_stream), torrent::internal_error);
    TEST_SL_ASSERT_CLOSED();
  };
  { TEST_SL_BEGIN("sin6_any, 0, 1000, 0");
    CPPUNIT_ASSERT_THROW(sl->open(torrent::sap_copy(sin6_any), 0, 1000, 0, torrent::fd_flag_stream), torrent::internal_error);
    TEST_SL_ASSERT_CLOSED();
  };
  { TEST_SL_BEGIN("sin6_any, 0, 0, 500");
    CPPUNIT_ASSERT_THROW(sl->open(torrent::sap_copy(sin6_any), 0, 0, 500, torrent::fd_flag_stream), torrent::internal_error);
    TEST_SL_ASSERT_CLOSED();
  };
  { TEST_SL_BEGIN("sin6_any, 0, 1000, 500");
    CPPUNIT_ASSERT_THROW(sl->open(torrent::sap_copy(sin6_any), 0, 1000, 500, torrent::fd_flag_stream), torrent::internal_error);
    TEST_SL_ASSERT_CLOSED();
  };
}

void
test_socket_listen::test_open_port_range() {
  { TEST_SL_BEGIN("sin6_any, stream, first");
    expect_fd_inet6_tcp(1000);
    expect_fd_bind_listen(1000, c_sin6_any_5000);
    TEST_SL_ASSERT_OPEN_PORT(torrent::sap_copy(sin6_any), c_sin6_any_5000, 5000, 5010, 5000, torrent::fd_flag_stream);
    TEST_SL_CLOSE(1000);
  };
  { TEST_SL_BEGIN("sin6_any, stream, from first to middle port");
    expect_fd_inet6_tcp(1000);
    TEST_SL_MOCK_CLOSED_PORT_RANGE(sin6_any, 5000, 5004);
    expect_fd_bind_listen(1000, c_sin6_any_5005);
    TEST_SL_ASSERT_OPEN_PORT(torrent::sap_copy(sin6_any), c_sin6_any_5005, 5000, 5010, 5000, torrent::fd_flag_stream);
    TEST_SL_CLOSE(1000);
  };
  { TEST_SL_BEGIN("sin6_any, stream, from first to last port");
    expect_fd_inet6_tcp(1000);
    TEST_SL_MOCK_CLOSED_PORT_RANGE(sin6_any, 5000, 5009);
    expect_fd_bind_listen(1000, c_sin6_any_5010);
    TEST_SL_ASSERT_OPEN_PORT(torrent::sap_copy(sin6_any), c_sin6_any_5010, 5000, 5010, 5000, torrent::fd_flag_stream);
    TEST_SL_CLOSE(1000);
  };
  { TEST_SL_BEGIN("sin6_any, stream, middle");
    expect_fd_inet6_tcp(1000);
    expect_fd_bind_listen(1000, c_sin6_any_5005);
    TEST_SL_ASSERT_OPEN_PORT(torrent::sap_copy(sin6_any), c_sin6_any_5005, 5000, 5010, 5005, torrent::fd_flag_stream);
    TEST_SL_CLOSE(1000);
  };
  { TEST_SL_BEGIN("sin6_any, stream, from middle to last port");
    expect_fd_inet6_tcp(1000);
    TEST_SL_MOCK_CLOSED_PORT_RANGE(sin6_any, 5005, 5009);
    expect_fd_bind_listen(1000, c_sin6_any_5010);
    TEST_SL_ASSERT_OPEN_PORT(torrent::sap_copy(sin6_any), c_sin6_any_5010, 5000, 5010, 5005, torrent::fd_flag_stream);
    TEST_SL_CLOSE(1000);
  };
  { TEST_SL_BEGIN("sin6_any, stream, last");
    expect_fd_inet6_tcp(1000);
    expect_fd_bind_listen(1000, c_sin6_any_5010);
    TEST_SL_ASSERT_OPEN_PORT(torrent::sap_copy(sin6_any), c_sin6_any_5010, 5000, 5010, 5010, torrent::fd_flag_stream);
    TEST_SL_CLOSE(1000);
  };
  { TEST_SL_BEGIN("sin6_any, stream, from last to first port");
    expect_fd_inet6_tcp(1000);
    TEST_SL_MOCK_CLOSED_PORT_RANGE(sin6_any, 5010, 5010);
    expect_fd_bind_listen(1000, c_sin6_any_5000);
    TEST_SL_ASSERT_OPEN_PORT(torrent::sap_copy(sin6_any), c_sin6_any_5000, 5000, 5010, 5010, torrent::fd_flag_stream);
    TEST_SL_CLOSE(1000);
  };
  { TEST_SL_BEGIN("sin6_any, stream, from last to middle port");
    expect_fd_inet6_tcp(1000);
    TEST_SL_MOCK_CLOSED_PORT_RANGE(sin6_any, 5010, 5010);
    TEST_SL_MOCK_CLOSED_PORT_RANGE(sin6_any, 5000, 5004);
    expect_fd_bind_listen(1000, c_sin6_any_5005);
    TEST_SL_ASSERT_OPEN_PORT(torrent::sap_copy(sin6_any), c_sin6_any_5005, 5000, 5010, 5010, torrent::fd_flag_stream);
    TEST_SL_CLOSE(1000);
  };
}

void
test_socket_listen::test_open_port_range_error() {
  { TEST_SL_BEGIN("sin6_any, first > last");
    CPPUNIT_ASSERT_THROW(sl->open(torrent::sap_copy(sin6_any), 5000, 4999, 5000, torrent::fd_flag_stream), torrent::internal_error);
    TEST_SL_ASSERT_CLOSED();
  };
  { TEST_SL_BEGIN("sin6_any, first > itr");
    CPPUNIT_ASSERT_THROW(sl->open(torrent::sap_copy(sin6_any), 5001, 5009, 5000, torrent::fd_flag_stream), torrent::internal_error);
    TEST_SL_ASSERT_CLOSED();
  };
  { TEST_SL_BEGIN("sin6_any, itr > last");
    CPPUNIT_ASSERT_THROW(sl->open(torrent::sap_copy(sin6_any), 5000, 5009, 5010, torrent::fd_flag_stream), torrent::internal_error);
    TEST_SL_ASSERT_CLOSED();
  };
  { TEST_SL_BEGIN("sin6_any, min first > last");
    CPPUNIT_ASSERT_THROW(sl->open(torrent::sap_copy(sin6_any), 2, 1, 2, torrent::fd_flag_stream), torrent::internal_error);
    TEST_SL_ASSERT_CLOSED();
  };
  { TEST_SL_BEGIN("sin6_any, min first > itr");
    CPPUNIT_ASSERT_THROW(sl->open(torrent::sap_copy(sin6_any), 2, 1000, 1, torrent::fd_flag_stream), torrent::internal_error);
    TEST_SL_ASSERT_CLOSED();
  };
  { TEST_SL_BEGIN("sin6_any, min itr > last");
    CPPUNIT_ASSERT_THROW(sl->open(torrent::sap_copy(sin6_any), 1, 2, 3, torrent::fd_flag_stream), torrent::internal_error);
    TEST_SL_ASSERT_CLOSED();
  };
  { TEST_SL_BEGIN("sin6_any, max first > last");
    CPPUNIT_ASSERT_THROW(sl->open(torrent::sap_copy(sin6_any), 0xffff, 0xfffe, 0xffff, torrent::fd_flag_stream), torrent::internal_error);
    TEST_SL_ASSERT_CLOSED();
  };
  { TEST_SL_BEGIN("sin6_any, max first > itr");
    CPPUNIT_ASSERT_THROW(sl->open(torrent::sap_copy(sin6_any), 0xffff, 0xffff, 0xfffe, torrent::fd_flag_stream), torrent::internal_error);
    TEST_SL_ASSERT_CLOSED();
  };
  { TEST_SL_BEGIN("sin6_any, max itr > last");
    CPPUNIT_ASSERT_THROW(sl->open(torrent::sap_copy(sin6_any), 0xfffe, 0xfffe, 0xffff, torrent::fd_flag_stream), torrent::internal_error);
    TEST_SL_ASSERT_CLOSED();
  };
}

void
test_socket_listen::test_open_sequential() {
  { TEST_SL_BEGIN("sin6_any, stream");
    expect_fd_inet6_tcp(1000);
    expect_fd_bind_listen(1000, c_sin6_any_5000);
    TEST_SL_ASSERT_OPEN_SEQUENTIAL(torrent::sap_copy(sin6_any), c_sin6_any_5000, 5000, 5010, torrent::fd_flag_stream);
    TEST_SL_CLOSE(1000);
  };
}

void
test_socket_listen::test_open_randomize() {
  { TEST_SL_BEGIN("sin6_any, stream");
    expect_random_uniform_uint16(5005, 5000, 5010);
    expect_fd_inet6_tcp(1000);
    expect_fd_bind_listen(1000, c_sin6_any_5005);
    TEST_SL_ASSERT_OPEN_RANDOMIZE(torrent::sap_copy(sin6_any), c_sin6_any_5005, 5000, 5010, torrent::fd_flag_stream);
    TEST_SL_CLOSE(1000);
  };
}

// deal with reuse error

void
test_socket_listen::test_accept() {
  { TEST_SL_BEGIN("sin6_any, stream");
    expect_fd_inet6_tcp(1000);
    expect_fd_bind_listen(1000, c_sin6_any_5000);
    TEST_SL_ASSERT_OPEN_SEQUENTIAL(torrent::sap_copy(sin6_any), c_sin6_any_5000, 5000, 5010, torrent::fd_flag_stream);

    std::vector<torrent::fd_sap_tuple> accepted_connections;

    sl->set_slot_accepted([&accepted_connections](int accept_fd, torrent::sa_unique_ptr sap) {
        accepted_connections.push_back(torrent::fd_sap_tuple{accept_fd, std::move(sap)});
      });

    // CPPUNIT_ASSERT(accepted_connections.size() > 0 && torrent::fd_sap_equal(accepted_connections[0], torrent::fd_sap_tuple{2000, torrent::sap_copy(sin6_1_5100)}));

    TEST_SL_CLOSE(1000);
  };
}

#include "config.h"

#include "test_bind_manager.h"

#include <fcntl.h>

#include "helpers/network.h"

#include "torrent/exceptions.h"
#include "torrent/net/bind_manager.h"
#include "torrent/net/fd.h"
#include "torrent/net/socket_address.h"
#include "torrent/utils/log.h"
#include "torrent/utils/random.h"

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(test_bind_manager, "torrent/net");

#define TEST_BM_BEGIN(name)                                     \
  torrent::bind_manager bm;                                     \
  std::vector<torrent::sa_unique_ptr> sap_cache;                \
  lt_log_print(torrent::LOG_MOCK_CALLS, "bm_begin: %s", name);  \
  TEST_DEFAULT_SA;

inline bool
compare_bind_base(const torrent::bind_struct& bs,
                  uint16_t priority,
                  int flags,
                  uint16_t listen_port_first = 0,
                  uint16_t listen_port_last = 0) {
  return
    priority == bs.priority &&
    flags == bs.flags &&
    listen_port_first == bs.listen_port_first &&
    listen_port_last == bs.listen_port_last;
}

inline bool
compare_listen_result(const torrent::listen_result_type& lhs, int rhs_fd, const torrent::c_sa_unique_ptr& rhs_sap) {
  return lhs.fd == rhs_fd &&
    ((lhs.address && rhs_sap) || ((lhs.address && rhs_sap) && torrent::sap_equal(lhs.address, rhs_sap)));
}

void
test_bind_manager::test_basic() {
  torrent::bind_manager bm;

  CPPUNIT_ASSERT(bm.flags() == torrent::bind_manager::flag_port_randomize);
  CPPUNIT_ASSERT(bm.listen_backlog() == SOMAXCONN);
  CPPUNIT_ASSERT(bm.listen_port() == 0);
  CPPUNIT_ASSERT(bm.listen_port_first() == 6881);
  CPPUNIT_ASSERT(bm.listen_port_last() == 6999);

  CPPUNIT_ASSERT(bm.empty());
  CPPUNIT_ASSERT(bm.size() == 0);
}

void
test_bind_manager::test_backlog() {
  torrent::bind_manager bm;

  CPPUNIT_ASSERT(bm.listen_backlog() == SOMAXCONN);

  CPPUNIT_ASSERT_NO_THROW(bm.set_listen_backlog(0));
  CPPUNIT_ASSERT(bm.listen_backlog() == 0);

  CPPUNIT_ASSERT_NO_THROW(bm.set_listen_backlog(SOMAXCONN - 1));
  CPPUNIT_ASSERT(bm.listen_backlog() == SOMAXCONN - 1);

  CPPUNIT_ASSERT_THROW(bm.set_listen_backlog(SOMAXCONN + 1), torrent::input_error);
  CPPUNIT_ASSERT(bm.listen_backlog() == SOMAXCONN - 1);

  CPPUNIT_ASSERT_THROW(bm.set_listen_backlog(-1), torrent::input_error);
  CPPUNIT_ASSERT(bm.listen_backlog() == SOMAXCONN - 1);
}

void
test_bind_manager::test_flags() {
  torrent::bind_manager bm;

  CPPUNIT_ASSERT(bm.flags() == torrent::bind_manager::flag_port_randomize);
  CPPUNIT_ASSERT(!bm.is_listen_open() && bm.is_port_randomize() && !bm.is_block_accept() && !bm.is_block_connect());

  bm.set_port_randomize(false);

  CPPUNIT_ASSERT(bm.flags() == 0);
  CPPUNIT_ASSERT(!bm.is_listen_open() && !bm.is_port_randomize() && !bm.is_block_accept() && !bm.is_block_connect());

  bm.set_block_accept(true);

  CPPUNIT_ASSERT(bm.flags() == torrent::bind_manager::flag_block_accept);
  CPPUNIT_ASSERT(!bm.is_listen_open() && !bm.is_port_randomize() && bm.is_block_accept() && !bm.is_block_connect());

  bm.set_block_connect(true);

  CPPUNIT_ASSERT(bm.flags() == (torrent::bind_manager::flag_block_accept | torrent::bind_manager::flag_block_connect));
  CPPUNIT_ASSERT(!bm.is_listen_open() && !bm.is_port_randomize() && bm.is_block_accept() && bm.is_block_connect());
}

void
test_bind_manager::test_add_bind() {
  torrent::bind_manager bm;

  CPPUNIT_ASSERT_NO_THROW(bm.add_bind("ipv4_zero", 100, wrap_ai_get_first_sa("0.0.0.0").get(), 0));

  CPPUNIT_ASSERT(bm.size() == 1);
  CPPUNIT_ASSERT(bm.back().name == "ipv4_zero");
  CPPUNIT_ASSERT(compare_bind_base(bm.back(), 100, torrent::bind_manager::flag_v4only));
  CPPUNIT_ASSERT(torrent::sap_equal_addr(bm.back().address, wrap_ai_get_first_sa("0.0.0.0")));
  CPPUNIT_ASSERT(torrent::sap_is_port_any(bm.back().address));

  CPPUNIT_ASSERT_NO_THROW(bm.add_bind("ipv6_zero", 100, wrap_ai_get_first_sa("::").get(), 0));

  CPPUNIT_ASSERT(bm.size() == 2);
  CPPUNIT_ASSERT(bm.back().name == "ipv6_zero");
  CPPUNIT_ASSERT(compare_bind_base(bm.back(), 100, 0));
  CPPUNIT_ASSERT(torrent::sap_equal_addr(bm.back().address, wrap_ai_get_first_sa("::")));
  CPPUNIT_ASSERT(torrent::sap_is_port_any(bm.back().address));

  CPPUNIT_ASSERT_NO_THROW(bm.add_bind("ipv4_lo", 100, wrap_ai_get_first_sa("127.0.0.1").get(), 0));

  CPPUNIT_ASSERT(bm.size() == 3);
  CPPUNIT_ASSERT(bm.back().name == "ipv4_lo");
  CPPUNIT_ASSERT(compare_bind_base(bm.back(), 100, torrent::bind_manager::flag_v4only));
  CPPUNIT_ASSERT(torrent::sap_equal_addr(bm.back().address, wrap_ai_get_first_sa("127.0.0.1")));
  CPPUNIT_ASSERT(torrent::sap_is_port_any(bm.back().address));

  CPPUNIT_ASSERT_NO_THROW(bm.add_bind("ipv6_lo", 100, wrap_ai_get_first_sa("ff01::1").get(), 0));

  CPPUNIT_ASSERT(bm.size() == 4);
  CPPUNIT_ASSERT(bm.back().name == "ipv6_lo");
  CPPUNIT_ASSERT(compare_bind_base(bm.back(), 100, torrent::bind_manager::flag_v6only));
  CPPUNIT_ASSERT(torrent::sap_equal_addr(bm.back().address, wrap_ai_get_first_sa("ff01::1")));
  CPPUNIT_ASSERT(torrent::sap_is_port_any(bm.back().address));
}

// TODO: Restrict characters in names.
// TODO: Test add_bind with flags.

void
test_bind_manager::test_add_bind_error() {
  torrent::bind_manager bm;

  CPPUNIT_ASSERT_NO_THROW(bm.add_bind("default", 100, wrap_ai_get_first_sa("0.0.0.0").get(), 0));
  CPPUNIT_ASSERT_THROW(bm.add_bind("default", 100, wrap_ai_get_first_sa("0.0.0.0").get(), 0), torrent::input_error);

  bm.clear();

  CPPUNIT_ASSERT_THROW(bm.add_bind("sa_nullptr", 100, nullptr, 0), torrent::internal_error);
  CPPUNIT_ASSERT_THROW(bm.add_bind("sa_unspec", 100, torrent::sa_make_unspec().get(), 0), torrent::internal_error);
  CPPUNIT_ASSERT_THROW(bm.add_bind("sa_unix", 100, torrent::sa_make_unix("").get(), 0), torrent::internal_error);
  CPPUNIT_ASSERT_THROW(bm.add_bind("sin_has_port", 100, wrap_ai_get_first_sa("0.0.0.0", "2000").get(), 0), torrent::input_error);
  CPPUNIT_ASSERT_THROW(bm.add_bind("sin6_has_port", 100, wrap_ai_get_first_sa("::", "2000").get(), 0), torrent::input_error);

  CPPUNIT_ASSERT_THROW(bm.add_bind("sin_v6only", 100, wrap_ai_get_first_sa("0.0.0.0").get(), torrent::bind_manager::flag_v6only), torrent::input_error);
  CPPUNIT_ASSERT_THROW(bm.add_bind("sin_bind_v6only", 100, wrap_ai_get_first_sa("1.2.3.4").get(), torrent::bind_manager::flag_v6only), torrent::input_error);
  CPPUNIT_ASSERT_THROW(bm.add_bind("sin6_v4only", 100, wrap_ai_get_first_sa("::").get(), torrent::bind_manager::flag_v4only), torrent::input_error);
  CPPUNIT_ASSERT_THROW(bm.add_bind("sin6_bind_v4only", 100, wrap_ai_get_first_sa("ff01::1").get(), torrent::bind_manager::flag_v4only), torrent::input_error);

  CPPUNIT_ASSERT_THROW(bm.add_bind("sin_v4only_v6only", 100, wrap_ai_get_first_sa("0.0.0.0").get(), torrent::bind_manager::flag_v4only | torrent::bind_manager::flag_v6only), torrent::input_error);
  CPPUNIT_ASSERT_THROW(bm.add_bind("sin6_v4only_v6only", 100, wrap_ai_get_first_sa("::").get(), torrent::bind_manager::flag_v4only | torrent::bind_manager::flag_v6only), torrent::input_error);

  CPPUNIT_ASSERT_THROW(bm.add_bind("sin_bc", 100, wrap_ai_get_first_sa("255.255.255.255").get(), 0), torrent::input_error);

  CPPUNIT_ASSERT(bm.empty());
}

void
test_bind_manager::test_add_bind_priority() {
  torrent::bind_manager bm;
  torrent::sa_unique_ptr sa = wrap_ai_get_first_sa("0.0.0.0");

  CPPUNIT_ASSERT_NO_THROW(bm.add_bind("c_1", 100, sa.get(), 0));
  CPPUNIT_ASSERT_NO_THROW(bm.add_bind("c_2", 100, sa.get(), 0));
  CPPUNIT_ASSERT_NO_THROW(bm.add_bind("a_3", 100, sa.get(), 0));
  CPPUNIT_ASSERT_NO_THROW(bm.add_bind("c_4", 0, sa.get(), 0));
  CPPUNIT_ASSERT_NO_THROW(bm.add_bind("a_5", 0, sa.get(), 0));
  CPPUNIT_ASSERT_NO_THROW(bm.add_bind("c_6", 200, sa.get(), 0));

  CPPUNIT_ASSERT(bm.size() == 6);
  CPPUNIT_ASSERT(bm[0].name == "c_4");
  CPPUNIT_ASSERT(bm[1].name == "a_5");
  CPPUNIT_ASSERT(bm[2].name == "c_1");
  CPPUNIT_ASSERT(bm[3].name == "c_2");
  CPPUNIT_ASSERT(bm[4].name == "a_3");
  CPPUNIT_ASSERT(bm[5].name == "c_6");
}

void
test_bind_manager::test_add_bind_v4mapped() {
  torrent::bind_manager bm;

  CPPUNIT_ASSERT_NO_THROW(bm.add_bind("v4mapped_zero", 100, wrap_ai_get_first_sa("::ffff:0.0.0.0").get(), 0));

  CPPUNIT_ASSERT(bm.back().name == "v4mapped_zero");
  CPPUNIT_ASSERT(compare_bind_base(bm.back(), 100, torrent::bind_manager::flag_v4only));
  CPPUNIT_ASSERT(torrent::sap_equal_addr(bm.back().address, torrent::sa_make_inet()));
  CPPUNIT_ASSERT(torrent::sap_is_port_any(bm.back().address));

  CPPUNIT_ASSERT_NO_THROW(bm.add_bind("v4mapped_1", 100, wrap_ai_get_first_sa("::ffff:1.2.3.4").get(), 0));

  CPPUNIT_ASSERT(bm.back().name == "v4mapped_1");
  CPPUNIT_ASSERT(compare_bind_base(bm.back(), 100, torrent::bind_manager::flag_v4only));
  CPPUNIT_ASSERT(torrent::sap_equal_addr(bm.back().address, wrap_ai_get_first_sa("1.2.3.4")));
  CPPUNIT_ASSERT(torrent::sap_is_port_any(bm.back().address));

  CPPUNIT_ASSERT_THROW(bm.add_bind("sin_v4mapped_bc", 100, wrap_ai_get_first_sa("::ffff:255.255.255.255").get(), 0), torrent::input_error);
}

void
test_bind_manager::test_remove_bind() {
  torrent::bind_manager bm;

  CPPUNIT_ASSERT(!bm.remove_bind("default"));
  CPPUNIT_ASSERT_THROW(bm.remove_bind_throw("default"), torrent::input_error);

  CPPUNIT_ASSERT_NO_THROW(bm.add_bind("default", 100, wrap_ai_get_first_sa("0.0.0.0").get(), 0));
  CPPUNIT_ASSERT(bm.remove_bind("default"));
  CPPUNIT_ASSERT(bm.size() == 0);
}

void
test_bind_manager::test_connect_socket() {
  { TEST_BM_BEGIN("sin6_any, connect sin_1_5000");
    CPPUNIT_ASSERT_NO_THROW(bm.add_bind("default", 100, sin6_any.get(), 0));
    mock_expect(&torrent::fd__socket, 1000, (int)PF_INET, (int)SOCK_STREAM, (int)IPPROTO_TCP);
    mock_expect(&torrent::fd__fcntl_int, 0, 1000, F_SETFL, O_NONBLOCK);
    mock_expect(&torrent::fd__connect, 0, 1000, c_sin_1_5000.get(), (socklen_t)sizeof(sockaddr_in));
    CPPUNIT_ASSERT(bm.connect_socket(sin_1_5000.get(), 0) == 1000);
  };
  { TEST_BM_BEGIN("sin6_any, connect sin6_1_5000");
    CPPUNIT_ASSERT_NO_THROW(bm.add_bind("default", 100, sin6_any.get(), 0));
    mock_expect(&torrent::fd__socket, 1000, (int)PF_INET6, (int)SOCK_STREAM, (int)IPPROTO_TCP);
    mock_expect(&torrent::fd__fcntl_int, 0, 1000, F_SETFL, O_NONBLOCK);
    mock_expect(&torrent::fd__connect, 0, 1000, c_sin6_1_5000.get(), (socklen_t)sizeof(sockaddr_in6));
    CPPUNIT_ASSERT(bm.connect_socket(sin6_1_5000.get(), 0) == 1000);
  };
  { TEST_BM_BEGIN("sin6_any, connect sin6_v4_1_5000");
    CPPUNIT_ASSERT_NO_THROW(bm.add_bind("default", 100, sin6_any.get(), 0));
    mock_expect(&torrent::fd__socket, 1000, (int)PF_INET, (int)SOCK_STREAM, (int)IPPROTO_TCP);
    mock_expect(&torrent::fd__fcntl_int, 0, 1000, F_SETFL, O_NONBLOCK);
    mock_expect(&torrent::fd__connect, 0, 1000, c_sin_1_5000.get(), (socklen_t)sizeof(sockaddr_in));
    CPPUNIT_ASSERT(bm.connect_socket(sin6_v4_1_5000.get(), 0) == 1000);
  };
}

void
test_bind_manager::test_connect_socket_v4bound() {
  { TEST_BM_BEGIN("sin_bnd, connect sin_1_5000");
    CPPUNIT_ASSERT_NO_THROW(bm.add_bind("default", 100, sin_bnd.get(), 0));
    mock_expect(&torrent::fd__socket, 1000, (int)PF_INET, (int)SOCK_STREAM, (int)IPPROTO_TCP);
    mock_expect(&torrent::fd__fcntl_int, 0, 1000, F_SETFL, O_NONBLOCK);
    mock_expect(&torrent::fd__bind, 0, 1000, c_sin_bnd.get(), (socklen_t)sizeof(sockaddr_in));
    mock_expect(&torrent::fd__connect, 0, 1000, c_sin_1_5000.get(), (socklen_t)sizeof(sockaddr_in));
    CPPUNIT_ASSERT(bm.connect_socket(sin_1_5000.get(), 0) == 1000);
  };
  { TEST_BM_BEGIN("sin_bnd, connect sin6_1_5000 fails");
    CPPUNIT_ASSERT_NO_THROW(bm.add_bind("default", 100, sin_bnd.get(), 0));
    CPPUNIT_ASSERT(bm.connect_socket(sin6_1_5000.get(), 0) == -1);
  }
  { TEST_BM_BEGIN("sin_bnd, connect sin6_v4_1_5000");
    CPPUNIT_ASSERT_NO_THROW(bm.add_bind("default", 100, sin_bnd.get(), 0));
    mock_expect(&torrent::fd__socket, 1000, (int)PF_INET, (int)SOCK_STREAM, (int)IPPROTO_TCP);
    mock_expect(&torrent::fd__fcntl_int, 0, 1000, F_SETFL, O_NONBLOCK);
    mock_expect(&torrent::fd__bind, 0, 1000, c_sin_bnd.get(), (socklen_t)sizeof(sockaddr_in));
    mock_expect(&torrent::fd__connect, 0, 1000, c_sin_1_5000.get(), (socklen_t)sizeof(sockaddr_in));
    CPPUNIT_ASSERT(bm.connect_socket(sin6_v4_1_5000.get(), 0) == 1000);
  };
}

// TODO: Use different address for bind and connect addresses.

void
test_bind_manager::test_connect_socket_v6bound() {
  { TEST_BM_BEGIN("sin6_bnd, connect sin_1_5000 fails");
    CPPUNIT_ASSERT_NO_THROW(bm.add_bind("default", 100, sin6_bnd.get(), 0));
    CPPUNIT_ASSERT(bm.connect_socket(sin_1_5000.get(), 0) == -1);
  };
  { TEST_BM_BEGIN("sin6_bnd, connect sin6_1_5000");
    CPPUNIT_ASSERT_NO_THROW(bm.add_bind("default", 100, sin6_bnd.get(), 0));
    mock_expect(&torrent::fd__socket, 1000, (int)PF_INET6, (int)SOCK_STREAM, (int)IPPROTO_TCP);
    mock_expect(&torrent::fd__setsockopt_int, 0, 1000, (int)IPPROTO_IPV6, (int)IPV6_V6ONLY, (int)true);
    mock_expect(&torrent::fd__fcntl_int, 0, 1000, F_SETFL, O_NONBLOCK);
    mock_expect(&torrent::fd__bind, 0, 1000, c_sin6_bnd.get(), (socklen_t)sizeof(sockaddr_in6));
    mock_expect(&torrent::fd__connect, 0, 1000, c_sin6_1_5000.get(), (socklen_t)sizeof(sockaddr_in6));
    CPPUNIT_ASSERT(bm.connect_socket(sin6_1_5000.get(), 0) == 1000);
  };
  { TEST_BM_BEGIN("sin6_bnd, connect sin6_v4_1_5000 fails");
    CPPUNIT_ASSERT_NO_THROW(bm.add_bind("default", 100, sin6_bnd.get(), 0));
    CPPUNIT_ASSERT(bm.connect_socket(sin6_v4_1_5000.get(), 0) == -1);
  };
}

void
test_bind_manager::test_connect_socket_v4only() {
  { TEST_BM_BEGIN("sin_any, connect sin_1_5000");
    CPPUNIT_ASSERT_NO_THROW(bm.add_bind("default", 100, sin_any.get(), torrent::bind_manager::flag_v4only));
    mock_expect(&torrent::fd__socket, 1000, (int)PF_INET, (int)SOCK_STREAM, (int)IPPROTO_TCP);
    mock_expect(&torrent::fd__fcntl_int, 0, 1000, F_SETFL, O_NONBLOCK);
    mock_expect(&torrent::fd__connect, 0, 1000, c_sin_1_5000.get(), (socklen_t)sizeof(sockaddr_in));
    CPPUNIT_ASSERT(bm.connect_socket(sin_1_5000.get(), 0) == 1000);
  };
  { TEST_BM_BEGIN("sin_any, connect sin6_1_5000 fails");
    CPPUNIT_ASSERT_NO_THROW(bm.add_bind("default", 100, sin_any.get(), torrent::bind_manager::flag_v4only));
    CPPUNIT_ASSERT(bm.connect_socket(sin6_1_5000.get(), 0) == -1);
  };
  { TEST_BM_BEGIN("sin_any, connect sin6_v4_1_5000");
    CPPUNIT_ASSERT_NO_THROW(bm.add_bind("default", 100, sin_any.get(), torrent::bind_manager::flag_v4only));
    mock_expect(&torrent::fd__socket, 1000, (int)PF_INET, (int)SOCK_STREAM, (int)IPPROTO_TCP);
    mock_expect(&torrent::fd__fcntl_int, 0, 1000, F_SETFL, O_NONBLOCK);
    mock_expect(&torrent::fd__connect, 0, 1000, c_sin_1_5000.get(), (socklen_t)sizeof(sockaddr_in));
    CPPUNIT_ASSERT(bm.connect_socket(sin6_v4_1_5000.get(), 0) == 1000);
  };
  { TEST_BM_BEGIN("sin_bnd, connect sin_1_5000");
    CPPUNIT_ASSERT_NO_THROW(bm.add_bind("default", 100, sin_bnd.get(), torrent::bind_manager::flag_v4only));
    mock_expect(&torrent::fd__socket, 1000, (int)PF_INET, (int)SOCK_STREAM, (int)IPPROTO_TCP);
    mock_expect(&torrent::fd__fcntl_int, 0, 1000, F_SETFL, O_NONBLOCK);
    mock_expect(&torrent::fd__bind, 0, 1000, c_sin_bnd.get(), (socklen_t)sizeof(sockaddr_in));
    mock_expect(&torrent::fd__connect, 0, 1000, c_sin_1_5000.get(), (socklen_t)sizeof(sockaddr_in));
    CPPUNIT_ASSERT(bm.connect_socket(sin_1_5000.get(), 0) == 1000);
  };
  { TEST_BM_BEGIN("sin_bnd, connect sin6_1_5000 fails");
    CPPUNIT_ASSERT_NO_THROW(bm.add_bind("default", 100, sin_1.get(), torrent::bind_manager::flag_v4only));
    CPPUNIT_ASSERT(bm.connect_socket(sin6_1_5000.get(), 0) == -1);
  };
  { TEST_BM_BEGIN("sin_bnd, connect sin6_v4_1_5000");
    CPPUNIT_ASSERT_NO_THROW(bm.add_bind("default", 100, sin_bnd.get(), torrent::bind_manager::flag_v4only));
    mock_expect(&torrent::fd__socket, 1000, (int)PF_INET, (int)SOCK_STREAM, (int)IPPROTO_TCP);
    mock_expect(&torrent::fd__fcntl_int, 0, 1000, F_SETFL, O_NONBLOCK);
    mock_expect(&torrent::fd__bind, 0, 1000, c_sin_bnd.get(), (socklen_t)sizeof(sockaddr_in));
    mock_expect(&torrent::fd__connect, 0, 1000, c_sin_1_5000.get(), (socklen_t)sizeof(sockaddr_in));
    CPPUNIT_ASSERT(bm.connect_socket(sin6_v4_1_5000.get(), 0) == 1000);
  };
}

void
test_bind_manager::test_connect_socket_v6only() {
  { TEST_BM_BEGIN("sin6_any, connect sin_1_5000");
    CPPUNIT_ASSERT_NO_THROW(bm.add_bind("default", 100, sin6_any.get(), torrent::bind_manager::flag_v6only));
    CPPUNIT_ASSERT(bm.connect_socket(sin_1_5000.get(), 0) == -1);
  };
  { TEST_BM_BEGIN("sin6_any, connect sin6_1_5000");
    CPPUNIT_ASSERT_NO_THROW(bm.add_bind("default", 100, sin6_any.get(), torrent::bind_manager::flag_v6only));
    mock_expect(&torrent::fd__socket, 1000, (int)PF_INET6, (int)SOCK_STREAM, (int)IPPROTO_TCP);
    mock_expect(&torrent::fd__setsockopt_int, 0, 1000, (int)IPPROTO_IPV6, (int)IPV6_V6ONLY, (int)true);
    mock_expect(&torrent::fd__fcntl_int, 0, 1000, F_SETFL, O_NONBLOCK);
    mock_expect(&torrent::fd__connect, 0, 1000, c_sin6_1_5000.get(), (socklen_t)sizeof(sockaddr_in6));
    CPPUNIT_ASSERT(bm.connect_socket(sin6_1_5000.get(), 0) == 1000);
  };
  { TEST_BM_BEGIN("sin6_any, connect sin6_v4_1_5000");
    CPPUNIT_ASSERT_NO_THROW(bm.add_bind("default", 100, sin6_any.get(), torrent::bind_manager::flag_v6only));
    CPPUNIT_ASSERT(bm.connect_socket(sin6_v4_1_5000.get(), 0) == -1);
  };
  { TEST_BM_BEGIN("sin6_bnd, connect sin_1_5000");
    CPPUNIT_ASSERT_NO_THROW(bm.add_bind("default", 100, sin6_bnd.get(), torrent::bind_manager::flag_v6only));
    CPPUNIT_ASSERT(bm.connect_socket(sin_1_5000.get(), 0) == -1);
  };
  { TEST_BM_BEGIN("sin6_bnd, connect sin6_1_5000");
    CPPUNIT_ASSERT_NO_THROW(bm.add_bind("default", 100, sin6_bnd.get(), torrent::bind_manager::flag_v6only));
    mock_expect(&torrent::fd__socket, 1000, (int)PF_INET6, (int)SOCK_STREAM, (int)IPPROTO_TCP);
    mock_expect(&torrent::fd__setsockopt_int, 0, 1000, (int)IPPROTO_IPV6, (int)IPV6_V6ONLY, (int)true);
    mock_expect(&torrent::fd__fcntl_int, 0, 1000, F_SETFL, O_NONBLOCK);
    mock_expect(&torrent::fd__bind, 0, 1000, c_sin6_bnd.get(), (socklen_t)sizeof(sockaddr_in6));
    mock_expect(&torrent::fd__connect, 0, 1000, c_sin6_1_5000.get(), (socklen_t)sizeof(sockaddr_in6));
    CPPUNIT_ASSERT(bm.connect_socket(sin6_1_5000.get(), 0) == 1000);
  };
  { TEST_BM_BEGIN("sin6_bnd, connect sin6_v4_1_5000");
    CPPUNIT_ASSERT_NO_THROW(bm.add_bind("default", 100, sin6_1.get(), torrent::bind_manager::flag_v6only));
    CPPUNIT_ASSERT(bm.connect_socket(sin6_v4_1_5000.get(), 0) == -1);
  };
}

void
test_bind_manager::test_connect_socket_block_connect() {
  { TEST_BM_BEGIN("sin6_any");
    CPPUNIT_ASSERT_NO_THROW(bm.add_bind("default", 100, sin6_any.get(), torrent::bind_manager::flag_block_connect));
    CPPUNIT_ASSERT(bm.connect_socket(sin_1_5000.get(), 0) == -1);
    CPPUNIT_ASSERT(bm.connect_socket(sin6_1_5000.get(), 0) == -1);
    CPPUNIT_ASSERT(bm.connect_socket(sin6_v4_1_5000.get(), 0) == -1);
  };
  { TEST_BM_BEGIN("sin_any");
    CPPUNIT_ASSERT_NO_THROW(bm.add_bind("default", 100, sin_any.get(), torrent::bind_manager::flag_block_connect));
    CPPUNIT_ASSERT(bm.connect_socket(sin_1_5000.get(), 0) == -1);
    CPPUNIT_ASSERT(bm.connect_socket(sin6_1_5000.get(), 0) == -1);
    CPPUNIT_ASSERT(bm.connect_socket(sin6_v4_1_5000.get(), 0) == -1);
  };
  { TEST_BM_BEGIN("sin6_1");
    CPPUNIT_ASSERT_NO_THROW(bm.add_bind("default", 100, sin6_1.get(), torrent::bind_manager::flag_block_connect));
    CPPUNIT_ASSERT(bm.connect_socket(sin_1_5000.get(), 0) == -1);
    CPPUNIT_ASSERT(bm.connect_socket(sin6_1_5000.get(), 0) == -1);
    CPPUNIT_ASSERT(bm.connect_socket(sin6_v4_1_5000.get(), 0) == -1);
  };
  { TEST_BM_BEGIN("sin_1");
    CPPUNIT_ASSERT_NO_THROW(bm.add_bind("default", 100, sin_1.get(), torrent::bind_manager::flag_block_connect));
    CPPUNIT_ASSERT(bm.connect_socket(sin_1_5000.get(), 0) == -1);
    CPPUNIT_ASSERT(bm.connect_socket(sin6_1_5000.get(), 0) == -1);
    CPPUNIT_ASSERT(bm.connect_socket(sin6_v4_1_5000.get(), 0) == -1);
  };
}

void
test_bind_manager::test_error_connect_socket() {
  { TEST_BM_BEGIN("sin6_any, connect zero port");
    CPPUNIT_ASSERT_NO_THROW(bm.add_bind("default", 100, sin6_any.get(), 0));
    CPPUNIT_ASSERT_THROW(bm.connect_socket(sin_1.get(), 0), torrent::internal_error);
    CPPUNIT_ASSERT_THROW(bm.connect_socket(sin6_1.get(), 0), torrent::internal_error);
    CPPUNIT_ASSERT_THROW(bm.connect_socket(sin6_v4_1.get(), 0), torrent::internal_error);
  };
  { TEST_BM_BEGIN("sin6_any, connect any address");
    CPPUNIT_ASSERT_NO_THROW(bm.add_bind("default", 100, sin6_any.get(), 0));
    CPPUNIT_ASSERT_THROW(bm.connect_socket(sin_any_5000.get(), 0), torrent::internal_error);
    CPPUNIT_ASSERT_THROW(bm.connect_socket(sin6_any_5000.get(), 0), torrent::internal_error);
    CPPUNIT_ASSERT_THROW(bm.connect_socket(sin6_v4_any_5000.get(), 0), torrent::internal_error);
  };
  { TEST_BM_BEGIN("sin6_any, connect any address, zero port");
    CPPUNIT_ASSERT_NO_THROW(bm.add_bind("default", 100, sin6_any.get(), 0));
    CPPUNIT_ASSERT_THROW(bm.connect_socket(sin_any.get(), 0), torrent::internal_error);
    CPPUNIT_ASSERT_THROW(bm.connect_socket(sin6_any.get(), 0), torrent::internal_error);
    CPPUNIT_ASSERT_THROW(bm.connect_socket(sin6_v4_any.get(), 0), torrent::internal_error);
  };
  { TEST_BM_BEGIN("sin6_any, connect broadcast address");
    CPPUNIT_ASSERT_NO_THROW(bm.add_bind("default", 100, sin6_any.get(), 0));
    CPPUNIT_ASSERT_THROW(bm.connect_socket(sin_bc_5000.get(), 0), torrent::internal_error);
  };
}

void
test_bind_manager::test_listen_socket_randomize() {
  { TEST_BM_BEGIN("sin6_any, first port");
    CPPUNIT_ASSERT_NO_THROW(bm.add_bind("default", 100, sin6_any.get(), 0));
    mock_expect(&torrent::fd__socket, 1000, (int)PF_INET6, (int)SOCK_STREAM, (int)IPPROTO_TCP);
    mock_expect(&torrent::fd__fcntl_int, 0, 1000, F_SETFL, O_NONBLOCK);
    mock_expect(&torrent::fd__setsockopt_int, 0, 1000, (int)SOL_SOCKET, (int)SO_REUSEADDR, (int)true);
    mock_expect(&torrent::random_uniform_uint16, (uint16_t)6881, (uint16_t)6881, (uint16_t)6999);
    mock_expect(&torrent::fd__bind, 0, 1000, c_sin6_any_6881.get(), (socklen_t)sizeof(sockaddr_in6));
    mock_expect(&torrent::fd__listen, 0, 1000, SOMAXCONN);
    CPPUNIT_ASSERT(compare_listen_result(bm.listen_socket(0), 1000, c_sin6_any_6881));
  };
  { TEST_BM_BEGIN("sin6_any, middle port");
    CPPUNIT_ASSERT_NO_THROW(bm.add_bind("default", 100, sin6_any.get(), 0));
    mock_expect(&torrent::fd__socket, 1000, (int)PF_INET6, (int)SOCK_STREAM, (int)IPPROTO_TCP);
    mock_expect(&torrent::fd__fcntl_int, 0, 1000, F_SETFL, O_NONBLOCK);
    mock_expect(&torrent::fd__setsockopt_int, 0, 1000, (int)SOL_SOCKET, (int)SO_REUSEADDR, (int)true);
    mock_expect(&torrent::random_uniform_uint16, (uint16_t)6900, (uint16_t)6881, (uint16_t)6999);
    mock_expect(&torrent::fd__bind, 0, 1000, c_sin6_any_6900.get(), (socklen_t)sizeof(sockaddr_in6));
    mock_expect(&torrent::fd__listen, 0, 1000, SOMAXCONN);
    CPPUNIT_ASSERT(compare_listen_result(bm.listen_socket(0), 1000, c_sin6_any_6900));
  };
  { TEST_BM_BEGIN("sin6_any, last port");
    CPPUNIT_ASSERT_NO_THROW(bm.add_bind("default", 100, sin6_any.get(), 0));
    mock_expect(&torrent::fd__socket, 1000, (int)PF_INET6, (int)SOCK_STREAM, (int)IPPROTO_TCP);
    mock_expect(&torrent::fd__fcntl_int, 0, 1000, F_SETFL, O_NONBLOCK);
    mock_expect(&torrent::fd__setsockopt_int, 0, 1000, (int)SOL_SOCKET, (int)SO_REUSEADDR, (int)true);
    mock_expect(&torrent::random_uniform_uint16, (uint16_t)6999, (uint16_t)6881, (uint16_t)6999);
    mock_expect(&torrent::fd__bind, 0, 1000, c_sin6_any_6999.get(), (socklen_t)sizeof(sockaddr_in6));
    mock_expect(&torrent::fd__listen, 0, 1000, SOMAXCONN);
    CPPUNIT_ASSERT(compare_listen_result(bm.listen_socket(0), 1000, c_sin6_any_6999));
  };
}

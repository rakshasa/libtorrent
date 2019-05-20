#include "config.h"

#include "test_bind_manager.h"

#include "helpers/mock_function.h"
#include "helpers/network.h"

#include "torrent/exceptions.h"
#include "torrent/net/bind_manager.h"
#include "torrent/net/fd.h"
#include "torrent/net/socket_address.h"

CPPUNIT_TEST_SUITE_REGISTRATION(test_bind_manager);

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

// TODO: Test add_bind with flags.
// TODO: Test remove bind.
// TODO: Restrict characters in names.
// TODO: Create fd_socket with attribute weak to mock fd_.

void
test_bind_manager::test_add_bind_error() {
  torrent::bind_manager bm;

  CPPUNIT_ASSERT_NO_THROW(bm.add_bind("default", 100, wrap_ai_get_first_sa("0.0.0.0").get(), 0));
  CPPUNIT_ASSERT_THROW(bm.add_bind("default", 100, wrap_ai_get_first_sa("0.0.0.0").get(), 0), torrent::input_error);

  bm.clear();

  CPPUNIT_ASSERT_THROW(bm.add_bind("sa_nullptr", 100, nullptr, 0), torrent::input_error);
  CPPUNIT_ASSERT_THROW(bm.add_bind("sa_unspec", 100, torrent::sa_make_unspec().get(), 0), torrent::input_error);
  CPPUNIT_ASSERT_THROW(bm.add_bind("sa_unix", 100, torrent::sa_make_unix("").get(), 0), torrent::input_error);
  CPPUNIT_ASSERT_THROW(bm.add_bind("sin_has_port", 100, wrap_ai_get_first_sa("0.0.0.0", "2000").get(), 0), torrent::input_error);
  CPPUNIT_ASSERT_THROW(bm.add_bind("sin6_has_port", 100, wrap_ai_get_first_sa("::", "2000").get(), 0), torrent::input_error);

  CPPUNIT_ASSERT_THROW(bm.add_bind("sin_v6only", 100, wrap_ai_get_first_sa("0.0.0.0").get(), torrent::bind_manager::flag_v6only), torrent::input_error);
  CPPUNIT_ASSERT_THROW(bm.add_bind("sin6_v4only", 100, wrap_ai_get_first_sa("::").get(), torrent::bind_manager::flag_v4only), torrent::input_error);
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

int
mock_fd__close(int fildes) {
  return mock_call<int>(&torrent::fd__close, fildes);
}

int
mock_fd__socket(int domain, int type, int protocol) {
  return mock_call<int>(&torrent::fd__socket, domain, type, protocol);
}

void
test_bind_manager::test_connect_socket() {
  // mock_init_map(&torrent::fd__close);
  // mock_expect(&torrent::fd__close, -1, 20);
  // mock_expect(&torrent::fd__close, 0, 10);

  // CPPUNIT_ASSERT(mock_fd__close(20) == -1);
  // CPPUNIT_ASSERT(mock_fd__close(10) == 0);

  mock_init_map(&torrent::fd__socket);
  mock_expect(&torrent::fd__socket, -1, 0, 0, 0);

  CPPUNIT_ASSERT(mock_fd__socket(0, 0, 0) == -1);

  // torrent::bind_manager bm;
  // bm.add_bind("default", 100, wrap_ai_get_first_sa("::").get(), 0);

  // CPPUNIT_ASSERT(bm.connect_socket(wrap_ai_get_first_sa("1.2.3.4").get(), 0) == -1);

  mock_cleanup_map(&torrent::fd__socket);
}

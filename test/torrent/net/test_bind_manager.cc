#include "config.h"

#include "test_bind_manager.h"

#include "helpers/network.h"

#include "torrent/exceptions.h"
#include "torrent/net/bind_manager.h"
#include "torrent/net/socket_address.h"

CPPUNIT_TEST_SUITE_REGISTRATION(test_bind_manager);

void
test_bind_manager::test_basic() {
  torrent::bind_manager bm;

  CPPUNIT_ASSERT(bm.flags() == torrent::bind_manager::flag_port_randomize);
  CPPUNIT_ASSERT(bm.listen_backlog() == SOMAXCONN);
  CPPUNIT_ASSERT(bm.listen_port() == 0);
  CPPUNIT_ASSERT(bm.listen_port_first() == 6881);
  CPPUNIT_ASSERT(bm.listen_port_last() == 6999);

  CPPUNIT_ASSERT(!bm.empty());
  CPPUNIT_ASSERT(bm.size() == 1);

  CPPUNIT_ASSERT(bm.front().name == "default");
  CPPUNIT_ASSERT(bm.front().flags == 0);
  CPPUNIT_ASSERT(bm.front().priority == 0);
  CPPUNIT_ASSERT(bm.front().listen_port_first == 0);
  CPPUNIT_ASSERT(bm.front().listen_port_first == 0);

  CPPUNIT_ASSERT(torrent::sap_is_any(bm.front().address));
  CPPUNIT_ASSERT(torrent::sap_is_inet6(bm.front().address));

  bm.clear();

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

  CPPUNIT_ASSERT_THROW(bm.add_bind("default", 100, wrap_ai_get_first_sa("0.0.0.0").get(), 0), torrent::input_error);

  bm.clear();

  CPPUNIT_ASSERT_THROW(bm.add_bind("fail1", 100, nullptr, 0), torrent::input_error);
  CPPUNIT_ASSERT_THROW(bm.add_bind("fail2", 100, torrent::sa_make_unspec().get(), 0), torrent::input_error);
  CPPUNIT_ASSERT_THROW(bm.add_bind("fail3", 100, wrap_ai_get_first_sa("0.0.0.0", "2000").get(), 0), torrent::input_error);
  CPPUNIT_ASSERT_THROW(bm.add_bind("fail4", 100, wrap_ai_get_first_sa("::", "2000").get(), 0), torrent::input_error);

  CPPUNIT_ASSERT(bm.empty());

  CPPUNIT_ASSERT_NO_THROW(bm.add_bind("ipv4_zero", 100, wrap_ai_get_first_sa("0.0.0.0").get(), 0));

  CPPUNIT_ASSERT(bm.size() == 1);
  CPPUNIT_ASSERT(bm.back().name == "ipv4_zero");
  CPPUNIT_ASSERT(bm.back().flags == torrent::bind_manager::flag_v4only);
  CPPUNIT_ASSERT(bm.back().priority == 100);
  CPPUNIT_ASSERT(bm.back().listen_port_first == 0);
  CPPUNIT_ASSERT(bm.back().listen_port_first == 0);
  CPPUNIT_ASSERT(torrent::sap_is_inet(bm.back().address));
  CPPUNIT_ASSERT(torrent::sap_is_any(bm.back().address));
  CPPUNIT_ASSERT(torrent::sap_is_port_any(bm.back().address));

  CPPUNIT_ASSERT_NO_THROW(bm.add_bind("ipv6_zero", 100, wrap_ai_get_first_sa("::").get(), 0));

  CPPUNIT_ASSERT(bm.size() == 2);
  CPPUNIT_ASSERT(bm.back().name == "ipv6_zero");
  CPPUNIT_ASSERT(bm.back().flags == 0);
  CPPUNIT_ASSERT(bm.back().priority == 100);
  CPPUNIT_ASSERT(bm.back().listen_port_first == 0);
  CPPUNIT_ASSERT(bm.back().listen_port_first == 0);
  CPPUNIT_ASSERT(torrent::sap_is_inet6(bm.back().address));
  CPPUNIT_ASSERT(torrent::sap_is_any(bm.back().address));
  CPPUNIT_ASSERT(torrent::sap_is_port_any(bm.back().address));

  CPPUNIT_ASSERT_NO_THROW(bm.add_bind("ipv4_lo", 100, wrap_ai_get_first_sa("127.0.0.1").get(), 0));

  CPPUNIT_ASSERT(bm.size() == 3);
  CPPUNIT_ASSERT(bm.back().name == "ipv4_lo");
  CPPUNIT_ASSERT(bm.back().flags == torrent::bind_manager::flag_v4only);
  CPPUNIT_ASSERT(bm.back().priority == 100);
  CPPUNIT_ASSERT(bm.back().listen_port_first == 0);
  CPPUNIT_ASSERT(bm.back().listen_port_first == 0);
  CPPUNIT_ASSERT(torrent::sap_is_inet(bm.back().address));
  CPPUNIT_ASSERT(!torrent::sap_is_any(bm.back().address));
  CPPUNIT_ASSERT(torrent::sap_equal_addr(bm.back().address, wrap_ai_get_first_sa("127.0.0.1")));
  CPPUNIT_ASSERT(torrent::sap_is_port_any(bm.back().address));

  CPPUNIT_ASSERT_NO_THROW(bm.add_bind("ipv6_lo", 100, wrap_ai_get_first_sa("::1").get(), 0));

  CPPUNIT_ASSERT(bm.size() == 4);
  CPPUNIT_ASSERT(bm.back().name == "ipv6_lo");
  CPPUNIT_ASSERT(bm.back().flags == 0);
  CPPUNIT_ASSERT(bm.back().priority == 100);
  CPPUNIT_ASSERT(bm.back().listen_port_first == 0);
  CPPUNIT_ASSERT(bm.back().listen_port_first == 0);
  CPPUNIT_ASSERT(torrent::sap_is_inet6(bm.back().address));
  CPPUNIT_ASSERT(!torrent::sap_is_any(bm.back().address));
  CPPUNIT_ASSERT(torrent::sap_equal_addr(bm.back().address, wrap_ai_get_first_sa("::1")));
  CPPUNIT_ASSERT(torrent::sap_is_port_any(bm.back().address));
}

// TODO: Test add_bind with flags.
// TODO: Test remove bind.
// TODO: Test with fake unix socket.
// TODO: Test add not lexical, do we sort by name?
// TODO: Add option to generate random default name.

void
test_bind_manager::test_priority() {
  torrent::bind_manager bm;

  // TODO: Add default priority.
  // TODO: Test no lexical ordering.

  // CPPUNIT_ASSERT_NO_THROW(bm.add_bind("test0_1", 0, wrap_ai_get_first_sa("127.0.0.1").get(), 0));

}

#include "config.h"

#include "test_bind_manager.h"

#include "torrent/exceptions.h"

#include "torrent/net/address_info.h"
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

  CPPUNIT_ASSERT(torrent::sa_is_any(bm.front().address.get()));
  CPPUNIT_ASSERT(torrent::sa_is_inet6(bm.front().address.get()));

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

// Get bindable ipv4/v6 addresses, allow passing env variables. Move to utilities.

void
test_bind_manager::test_add_bind() {
  torrent::bind_manager bm;

  CPPUNIT_ASSERT_THROW(bm.add_bind("default", 0, torrent::ai_get_first_sa("0.0.0.0").get(), 0), torrent::input_error);

  bm.clear();

  CPPUNIT_ASSERT_THROW(bm.add_bind("fail1", 0, nullptr, 0), torrent::input_error);
  CPPUNIT_ASSERT_THROW(bm.add_bind("fail2", 0, torrent::sa_make_unspec().get(), 0), torrent::input_error);
  CPPUNIT_ASSERT_THROW(bm.add_bind("fail3", 0, torrent::ai_get_first_sa("0.0.0.0", "2000").get(), 0), torrent::input_error);
  CPPUNIT_ASSERT_THROW(bm.add_bind("fail4", 0, torrent::ai_get_first_sa("::", "2000").get(), 0), torrent::input_error);
  // Test with fake unix socket.

  CPPUNIT_ASSERT_NO_THROW(bm.add_bind("test1", 0, torrent::ai_get_first_sa("0.0.0.0").get(), 0));

  CPPUNIT_ASSERT(bm.size() == 1);

  CPPUNIT_ASSERT(bm.back().name == "test1");
  CPPUNIT_ASSERT(bm.back().flags == torrent::bind_manager::flag_v4only);
  CPPUNIT_ASSERT(bm.back().priority == 0);
  CPPUNIT_ASSERT(bm.back().listen_port_first == 0);
  CPPUNIT_ASSERT(bm.back().listen_port_first == 0);

  CPPUNIT_ASSERT(torrent::sa_is_inet(bm.back().address.get()));
  CPPUNIT_ASSERT(torrent::sa_is_any(bm.back().address.get()));
  CPPUNIT_ASSERT(torrent::sa_is_port_any(bm.back().address.get()));
}

// Test priority.

#include "config.h"

#include "test_network_config.h"

#include "runtime_manager.h"
#include "torrent/net/socket_address.h"
#include "torrent/runtime/network_config.h"
#include "torrent/system/scheduler.h"

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(test_network_config, "torrent/runtime");

void
test_network_config::test_destroy_with_pending_notification() {
  auto scheduler = torrent::this_thread::scheduler();

  // The setter schedules the change notification 200 ms later.
  torrent::runtime::network_config()->set_block_ipv6(true);
  CPPUNIT_ASSERT(scheduler->next_timeout(1h) < 1h);

  torrent::RuntimeManager::destroy();

  // tearDown() needs a runtime.
  torrent::RuntimeManager::initialize();

  CPPUNIT_ASSERT(scheduler->next_timeout(1h) == 1h);
}

void
test_network_config::test_set_bind_inet6_address() {
  auto* nc = torrent::runtime::network_config();

  CPPUNIT_ASSERT_NO_THROW(nc->set_bind_address("::"));
  CPPUNIT_ASSERT(torrent::sa_is_inet6(nc->bind_inet6_address().get()));
  CPPUNIT_ASSERT(torrent::sa_is_unspec(nc->bind_inet_address().get()));

  CPPUNIT_ASSERT_NO_THROW(nc->set_bind_address("2001:db8::1"));
  CPPUNIT_ASSERT(torrent::sa_is_inet6(nc->bind_inet6_address().get()));

  nc->set_bind_address("");
  CPPUNIT_ASSERT(torrent::sa_is_unspec(nc->bind_inet6_address().get()));
}

void
test_network_config::test_set_local_inet6_address() {
  auto* nc = torrent::runtime::network_config();

  CPPUNIT_ASSERT_NO_THROW(nc->set_local_address("2001:db8::1"));
  CPPUNIT_ASSERT(torrent::sa_is_inet6(nc->local_inet6_address().get()));
}

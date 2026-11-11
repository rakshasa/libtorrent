#include "config.h"

#include "test_network_config.h"

#include "runtime_manager.h"
#include "torrent/net/socket_address.h"
#include "torrent/runtime/network_config.h"
#include "torrent/system/scheduler.h"
#include "tracker/udp_router.h"

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

void
test_network_config::test_udp_bind_addresses() {
  auto* nc = torrent::runtime::network_config();
  auto udp = [nc](int family) { return nc->bind_address_for_udp_connect(family).first; };

  CPPUNIT_ASSERT(torrent::sa_is_inet(udp(AF_INET).get()));
  CPPUNIT_ASSERT(torrent::sa_is_inet6(udp(AF_INET6).get()));

  nc->set_bind_inet6_device_name("test-device");
  nc->set_bind_address("::");
  CPPUNIT_ASSERT(torrent::sa_is_inet6(nc->bind_address_for_tcp_connect(AF_INET).first.get()));
  CPPUNIT_ASSERT(torrent::sa_is_inet(udp(AF_INET).get()));
  CPPUNIT_ASSERT(torrent::sa_is_any(udp(AF_INET).get()));
  CPPUNIT_ASSERT(nc->bind_address_for_udp_connect(AF_INET).second == "test-device");
  CPPUNIT_ASSERT(torrent::sa_is_inet6(udp(AF_INET6).get()));

  nc->set_block_ipv4in6(true);
  CPPUNIT_ASSERT(udp(AF_INET) == nullptr);
  CPPUNIT_ASSERT(torrent::sa_is_inet6(udp(AF_INET6).get()));
  nc->set_block_ipv4in6(false);

  nc->set_bind_address("2001:db8::1");
  CPPUNIT_ASSERT(udp(AF_INET) == nullptr);
  CPPUNIT_ASSERT(torrent::sa_is_inet6(udp(AF_INET6).get()));

  nc->set_bind_address("::ffff:127.0.0.1");
  CPPUNIT_ASSERT(torrent::sa_is_inet(udp(AF_INET).get()));
  CPPUNIT_ASSERT(torrent::sa_addr_str(udp(AF_INET).get()) == "127.0.0.1");

  nc->set_bind_inet_address("127.0.0.1");
  CPPUNIT_ASSERT(torrent::sa_is_inet(udp(AF_INET).get()));
  nc->set_block_ipv4(true);
  CPPUNIT_ASSERT(udp(AF_INET) == nullptr);
  nc->set_block_ipv6(true);
  CPPUNIT_ASSERT(udp(AF_INET6) == nullptr);
  nc->set_block_ipv4(false);
  nc->set_block_ipv6(false);
  nc->set_bind_address("127.0.0.1");
  CPPUNIT_ASSERT(torrent::sa_is_inet(udp(AF_INET).get()));
  CPPUNIT_ASSERT(udp(AF_INET6) == nullptr);
}

void
test_network_config::test_udp_router_bind_changes() {
  auto* nc = torrent::runtime::network_config();
  torrent::tracker::UdpRouter router;

  router.open(AF_INET);
  CPPUNIT_ASSERT(router.is_open());

  nc->set_bind_address("::1");
  CPPUNIT_ASSERT_NO_THROW(router.updated_network_config(AF_INET));
  CPPUNIT_ASSERT(!router.is_open());

  nc->set_bind_address("::");
  CPPUNIT_ASSERT_NO_THROW(router.updated_network_config(AF_INET));
  CPPUNIT_ASSERT(router.is_open());

  nc->set_block_ipv4in6(true);
  CPPUNIT_ASSERT_NO_THROW(router.updated_network_config(AF_INET));
  CPPUNIT_ASSERT(!router.is_open());

  nc->set_block_ipv4in6(false);
  CPPUNIT_ASSERT_NO_THROW(router.updated_network_config(AF_INET));
  CPPUNIT_ASSERT(router.is_open());
  router.close();

  CPPUNIT_ASSERT_NO_THROW(router.open(AF_INET));
  CPPUNIT_ASSERT(router.is_open());
  router.close();
}

#include "config.h"

#include "test_dht_controller.h"

#include <arpa/inet.h>

#include "torrent/hash_string.h"
#include "torrent/net/socket_address.h"
#include "torrent/object.h"
#include "torrent/runtime/network_config.h"
#include "torrent/runtime/network_manager.h"
#include "torrent/runtime/socket_manager.h"
#include "dht/dht_tracker.h"
#include "torrent/tracker/dht_controller.h"

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(test_dht_controller, "torrent");

namespace {

constexpr uint16_t     dht_port = 43821;
constexpr unsigned int bootstrap_complete_nodes = 32;

torrent::Object
create_dht_cache(unsigned int node_count) {
  torrent::HashString self_id;
  self_id.clear(0x55);

  auto cache = torrent::Object::create_map();
  cache.insert_key("self_id", self_id.str());

  auto& nodes = cache.insert_key("nodes", torrent::Object::create_map());

  for (unsigned int i = 0; i < node_count; i++) {
    torrent::HashString node_id = self_id;
    node_id.data()[i / 8] ^= 0x80 >> (i % 8);

    auto& node = nodes.insert_key(node_id.str(), torrent::Object::create_map());

    node.insert_key("i", int64_t{0x7f000001});
    node.insert_key("p", int64_t{10000 + i});
    node.insert_key("t", int64_t{0});
  }

  return cache;
}

torrent::sa_unique_ptr
make_inet6(const char* addr, uint16_t port) {
  auto sin6 = torrent::sin6_make();
  inet_pton(AF_INET6, addr, &sin6->sin6_addr);
  sin6->sin6_port = htons(port);
  return torrent::sa_from_in6(std::move(sin6));
}

// queries sent by the IPv4 and the IPv6 router after one node is handed over the way a peer's PORT message does
std::pair<unsigned int, unsigned int>
add_peer_node_and_count_queries_both(const sockaddr* sa) {
  auto dht = torrent::runtime::network_manager()->dht_controller();
  auto before = dht->get_statistics();

  torrent::runtime::network_manager()->dht_add_peer_node(sa, 6881);

  auto after = dht->get_statistics();
  return {after.queries_sent - before.queries_sent, after.queries_sent6 - before.queries_sent6};
}

unsigned int
add_peer_node_and_count_queries() {
  auto dht = torrent::runtime::network_manager()->dht_controller();

  auto queries_sent = dht->get_statistics().queries_sent;
  auto sa = torrent::sa_make_inet_h(0x7f000002, 0);

  torrent::runtime::network_manager()->dht_add_peer_node(sa.get(), 6881);

  return dht->get_statistics().queries_sent - queries_sent;
}

} // namespace

void
test_dht_controller::setUp() {
  TestFixtureWithMainNetTrackerThread::setUp();

  torrent::runtime::socket_manager()->set_max_size_and_adjust(1024);
  torrent::runtime::network_config()->set_override_dht_port(dht_port);
}

void
test_dht_controller::tearDown() {
  torrent::runtime::network_manager()->dht_controller()->stop();

  TestFixtureWithMainNetTrackerThread::tearDown();
}

void
test_dht_controller::test_add_peer_node_while_bootstrapping() {
  auto dht = torrent::runtime::network_manager()->dht_controller();

  CPPUNIT_ASSERT(!dht->is_nodes_populated());

  dht->initialize(create_dht_cache(0));

  CPPUNIT_ASSERT(dht->start());
  CPPUNIT_ASSERT_EQUAL(0u, dht->get_statistics().num_nodes);
  CPPUNIT_ASSERT(!dht->is_nodes_populated());

  CPPUNIT_ASSERT_EQUAL(1u, add_peer_node_and_count_queries());
}

void
test_dht_controller::test_add_peer_node_after_bootstrap() {
  auto dht = torrent::runtime::network_manager()->dht_controller();

  dht->initialize(create_dht_cache(bootstrap_complete_nodes));

  CPPUNIT_ASSERT(dht->start());
  CPPUNIT_ASSERT_EQUAL(bootstrap_complete_nodes, dht->get_statistics().num_nodes);
  CPPUNIT_ASSERT(dht->is_nodes_populated());

  CPPUNIT_ASSERT_EQUAL(0u, add_peer_node_and_count_queries());
}

void
test_dht_controller::test_add_peer_stores_compact_port_in_network_order() {
  torrent::DhtTracker tracker;
  auto sa = torrent::sa_make_inet_h(0x01020304, 0);
  auto sin = reinterpret_cast<const sockaddr_in*>(sa.get());

  tracker.add_peer(sin->sin_addr.s_addr, 1000);

  torrent::raw_list peers = tracker.get_peers(1);
  CPPUNIT_ASSERT_EQUAL(uint32_t{8}, peers.size());

  auto data = reinterpret_cast<const unsigned char*>(peers.data());

  CPPUNIT_ASSERT_EQUAL(static_cast<unsigned char>('6'), data[0]);
  CPPUNIT_ASSERT_EQUAL(static_cast<unsigned char>(':'), data[1]);
  CPPUNIT_ASSERT_EQUAL(static_cast<unsigned char>(0x01), data[2]);
  CPPUNIT_ASSERT_EQUAL(static_cast<unsigned char>(0x02), data[3]);
  CPPUNIT_ASSERT_EQUAL(static_cast<unsigned char>(0x03), data[4]);
  CPPUNIT_ASSERT_EQUAL(static_cast<unsigned char>(0x04), data[5]);
  CPPUNIT_ASSERT_EQUAL(static_cast<unsigned char>(0x03), data[6]);
  CPPUNIT_ASSERT_EQUAL(static_cast<unsigned char>(0xE8), data[7]);
}

void
test_dht_controller::test_inet6_router_starts_beside_inet() {
  auto dht = torrent::runtime::network_manager()->dht_controller();

  dht->initialize(create_dht_cache(0));

  CPPUNIT_ASSERT(dht->start());
  CPPUNIT_ASSERT(dht->is_active());

  auto stats = dht->get_statistics();
  CPPUNIT_ASSERT(stats.cycle >= 1);
  CPPUNIT_ASSERT(stats.active6);
  CPPUNIT_ASSERT_EQUAL(0u, stats.num_nodes6);
}

void
test_dht_controller::test_inet6_peer_node_goes_to_the_inet6_router() {
  auto dht = torrent::runtime::network_manager()->dht_controller();

  dht->initialize(create_dht_cache(0));
  CPPUNIT_ASSERT(dht->start());

  auto v6 = make_inet6("2001:db8::1", 0);
  auto [q4, q6] = add_peer_node_and_count_queries_both(v6.get());
  CPPUNIT_ASSERT_EQUAL(0u, q4);
  CPPUNIT_ASSERT_EQUAL(1u, q6);

  // a v4-mapped address is an IPv4 node
  auto mapped = make_inet6("::ffff:127.0.0.3", 0);
  auto [m4, m6] = add_peer_node_and_count_queries_both(mapped.get());
  CPPUNIT_ASSERT_EQUAL(1u, m4);
  CPPUNIT_ASSERT_EQUAL(0u, m6);
}

void
test_dht_controller::test_inet6_peer_node_while_inet_is_populated() {
  auto dht = torrent::runtime::network_manager()->dht_controller();

  // the IPv4 table is full enough, the IPv6 one is empty: a PORT message from an IPv6 peer still counts
  dht->initialize(create_dht_cache(bootstrap_complete_nodes));
  CPPUNIT_ASSERT(dht->start());
  CPPUNIT_ASSERT(dht->is_nodes_populated());

  auto v4 = torrent::sa_make_inet_h(0x7f000002, 0);
  auto [q4, q4_6] = add_peer_node_and_count_queries_both(v4.get());
  CPPUNIT_ASSERT_EQUAL(0u, q4);
  CPPUNIT_ASSERT_EQUAL(0u, q4_6);

  auto v6 = make_inet6("2001:db8::2", 0);
  auto [q6_4, q6] = add_peer_node_and_count_queries_both(v6.get());
  CPPUNIT_ASSERT_EQUAL(0u, q6_4);
  CPPUNIT_ASSERT_EQUAL(1u, q6);
}

void
test_dht_controller::test_inet6_blocked_keeps_inet6_router_stopped() {
  auto dht = torrent::runtime::network_manager()->dht_controller();

  torrent::runtime::network_config()->set_block_ipv6(true);

  dht->initialize(create_dht_cache(0));
  CPPUNIT_ASSERT(dht->start());

  auto stats = dht->get_statistics();
  torrent::runtime::network_config()->set_block_ipv6(false);

  CPPUNIT_ASSERT(!stats.active6);
  CPPUNIT_ASSERT(stats.cycle >= 1);
}

void
test_dht_controller::test_cache_keeps_inet6_nodes() {
  auto dht = torrent::runtime::network_manager()->dht_controller();

  auto cache = create_dht_cache(2);
  auto& nodes6 = cache.insert_key("nodes6", torrent::Object::create_map());

  for (unsigned int i = 0; i < 3; i++) {
    torrent::HashString node_id;
    node_id.clear(0x33);
    node_id.data()[0] = static_cast<char>(0x40 + i);

    auto& node = nodes6.insert_key(node_id.str(), torrent::Object::create_map());
    auto addr = make_inet6("2001:db8::10", 0);
    auto sin6 = reinterpret_cast<const sockaddr_in6*>(addr.get());

    std::string raw(reinterpret_cast<const char*>(&sin6->sin6_addr), 16);
    raw[15] = static_cast<char>(0x10 + i);

    node.insert_key("i6", raw);
    node.insert_key("p", int64_t{20000 + i});
    node.insert_key("t", int64_t{0});
  }

  dht->initialize(cache);

  auto stats = dht->get_statistics();
  CPPUNIT_ASSERT_EQUAL(2u, stats.num_nodes);
  CPPUNIT_ASSERT_EQUAL(3u, stats.num_nodes6);

  auto out = torrent::Object::create_map();
  dht->store_cache(&out);

  CPPUNIT_ASSERT_EQUAL(size_t{2}, out.get_key_map("nodes").size());
  CPPUNIT_ASSERT_EQUAL(size_t{3}, out.get_key_map("nodes6").size());
}

#include "config.h"

#include "test_dht_controller.h"

#include "torrent/hash_string.h"
#include "torrent/net/socket_address.h"
#include "torrent/object.h"
#include "torrent/runtime/network_config.h"
#include "torrent/runtime/network_manager.h"
#include "torrent/runtime/socket_manager.h"
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

#include "helpers/test_main_thread.h"

class test_dht_controller : public TestFixtureWithMainNetTrackerThread {
  CPPUNIT_TEST_SUITE(test_dht_controller);
  CPPUNIT_TEST(test_add_peer_node_while_bootstrapping);
  CPPUNIT_TEST(test_add_peer_node_after_bootstrap);
  CPPUNIT_TEST(test_add_peer_stores_compact_port_in_network_order);
  CPPUNIT_TEST(test_inet6_router_starts_beside_inet);
  CPPUNIT_TEST(test_inet6_peer_node_goes_to_the_inet6_router);
  CPPUNIT_TEST(test_inet6_peer_node_while_inet_is_populated);
  CPPUNIT_TEST(test_inet6_blocked_keeps_inet6_router_stopped);
  CPPUNIT_TEST(test_cache_keeps_inet6_nodes);
  CPPUNIT_TEST_SUITE_END();

public:
  void setUp() override;
  void tearDown() override;

  void test_add_peer_node_while_bootstrapping();
  void test_add_peer_node_after_bootstrap();
  void test_add_peer_stores_compact_port_in_network_order();
  void test_inet6_router_starts_beside_inet();
  void test_inet6_peer_node_goes_to_the_inet6_router();
  void test_inet6_peer_node_while_inet_is_populated();
  void test_inet6_blocked_keeps_inet6_router_stopped();
  void test_cache_keeps_inet6_nodes();
};

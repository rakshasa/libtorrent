#include "helpers/test_main_thread.h"

class test_dht_controller : public TestFixtureWithMainNetTrackerThread {
  CPPUNIT_TEST_SUITE(test_dht_controller);
  CPPUNIT_TEST(test_add_peer_node_while_bootstrapping);
  CPPUNIT_TEST(test_add_peer_node_after_bootstrap);
  CPPUNIT_TEST_SUITE_END();

public:
  void setUp() override;
  void tearDown() override;

  void test_add_peer_node_while_bootstrapping();
  void test_add_peer_node_after_bootstrap();
};

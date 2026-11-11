#include "helpers/test_main_thread.h"

class test_network_config : public TestFixtureWithMainThread {
  CPPUNIT_TEST_SUITE(test_network_config);
  CPPUNIT_TEST(test_destroy_with_pending_notification);
  CPPUNIT_TEST(test_set_bind_inet6_address);
  CPPUNIT_TEST(test_set_local_inet6_address);
  CPPUNIT_TEST(test_udp_bind_addresses);
  CPPUNIT_TEST(test_udp_router_bind_changes);
  CPPUNIT_TEST_SUITE_END();

public:
  void test_destroy_with_pending_notification();
  void test_set_bind_inet6_address();
  void test_set_local_inet6_address();
  void test_udp_bind_addresses();
  void test_udp_router_bind_changes();
};

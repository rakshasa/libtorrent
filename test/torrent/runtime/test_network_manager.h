#include "helpers/test_fixture.h"

class test_network_manager : public test_fixture {
  CPPUNIT_TEST_SUITE(test_network_manager);
  CPPUNIT_TEST(test_listen_port_change_before_network);
  CPPUNIT_TEST(test_listen_port_change_with_proxy);
  CPPUNIT_TEST(test_listen_port_change);
  CPPUNIT_TEST(test_listen_port_change_ipv4_only);
  CPPUNIT_TEST_SUITE_END();

public:
  void setUp() override;
  void tearDown() override;

  void test_listen_port_change_before_network();
  void test_listen_port_change_with_proxy();
  void test_listen_port_change();
  void test_listen_port_change_ipv4_only();
};

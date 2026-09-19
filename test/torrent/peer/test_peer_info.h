#include "test/helpers/test_main_thread.h"

class test_peer_info : public TestFixtureWithMainThread {
  CPPUNIT_TEST_SUITE(test_peer_info);

  CPPUNIT_TEST(test_id_hex_is_terminated_on_construction);

  CPPUNIT_TEST_SUITE_END();

public:
  void test_id_hex_is_terminated_on_construction();
};

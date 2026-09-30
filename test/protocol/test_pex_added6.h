#include "test/helpers/test_main_thread.h"

class TestPexAdded6 : public TestFixtureWithMainThread {
  CPPUNIT_TEST_SUITE(TestPexAdded6);

  CPPUNIT_TEST(test_pex_added6_parses_to_inet6_addresses);

  CPPUNIT_TEST_SUITE_END();

public:
  void test_pex_added6_parses_to_inet6_addresses();
};

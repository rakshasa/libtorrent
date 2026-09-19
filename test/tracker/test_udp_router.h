#include "helpers/test_fixture.h"

class test_udp_router : public test_fixture {
  CPPUNIT_TEST_SUITE(test_udp_router);
  CPPUNIT_TEST(test_random_engine_state);
  CPPUNIT_TEST(test_random_engine_seeding);
  CPPUNIT_TEST_SUITE_END();

public:
  void test_random_engine_state();
  void test_random_engine_seeding();
};

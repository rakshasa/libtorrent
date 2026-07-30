#include "helpers/test_fixture.h"

class test_socket_manager : public test_fixture {
  CPPUNIT_TEST_SUITE(test_socket_manager);

  CPPUNIT_TEST(test_basic);
  CPPUNIT_TEST(test_adjust_allocation_over_budget);
  CPPUNIT_TEST(test_adjust_allocation_staged_min_alloc);

  CPPUNIT_TEST_SUITE_END();

public:
  void setUp() override;
  void tearDown() override;

  void test_basic();
  void test_adjust_allocation_over_budget();
  void test_adjust_allocation_staged_min_alloc();
};

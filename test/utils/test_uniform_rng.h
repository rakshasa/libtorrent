#include <cppunit/extensions/HelperMacros.h>

class TestUniformRNG : public CppUnit::TestFixture {
  CPPUNIT_TEST_SUITE(TestUniformRNG);
  CPPUNIT_TEST(test_rand);
  CPPUNIT_TEST(test_rand_range);
  CPPUNIT_TEST(test_rand_below);
  CPPUNIT_TEST_SUITE_END();

public:
  void setUp() {}
  void tearDown() {}

  void test_rand();
  void test_rand_range();
  void test_rand_below();
};

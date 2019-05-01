#include <cppunit/extensions/HelperMacros.h>

class test_bind_manager : public CppUnit::TestFixture {
  CPPUNIT_TEST_SUITE(test_bind_manager);
  CPPUNIT_TEST(test_basic);
  CPPUNIT_TEST(test_backlog);
  CPPUNIT_TEST(test_flags);

  CPPUNIT_TEST(test_add_bind);
  CPPUNIT_TEST(test_priority);
  CPPUNIT_TEST_SUITE_END();

public:
  void setUp() {}
  void tearDown() {}

  void test_basic();
  void test_backlog();
  void test_flags();

  void test_add_bind();
  void test_priority();
};

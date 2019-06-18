#include <cppunit/extensions/HelperMacros.h>

class test_socket_address : public CppUnit::TestFixture {
  CPPUNIT_TEST_SUITE(test_socket_address);

  CPPUNIT_TEST(test_sa_is_any);
  CPPUNIT_TEST(test_sa_is_broadcast);

  CPPUNIT_TEST(test_make);

  CPPUNIT_TEST(test_sin_from_sa);
  CPPUNIT_TEST(test_sin6_from_sa);

  CPPUNIT_TEST(test_sa_equal);
  CPPUNIT_TEST(test_sa_equal_addr);
  CPPUNIT_TEST(test_sa_copy);
  CPPUNIT_TEST(test_sa_copy_addr);

  CPPUNIT_TEST(test_sa_from_v4mapped);
  CPPUNIT_TEST(test_sa_to_v4mapped);

  CPPUNIT_TEST_SUITE_END();

public:
  void setUp() {}
  void tearDown() {}

  void test_sa_is_any();
  void test_sa_is_broadcast();

  void test_make();

  void test_sin_from_sa();
  void test_sin6_from_sa();

  void test_sa_equal();
  void test_sa_equal_addr();
  void test_sa_copy();
  void test_sa_copy_addr();

  void test_sa_from_v4mapped();
  void test_sa_to_v4mapped();
};

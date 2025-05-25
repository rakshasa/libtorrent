#include "helpers/test_main_thread.h"

class TestSocketListen : public TestFixtureWithMockAndMainThread {
  CPPUNIT_TEST_SUITE(TestSocketListen);

  CPPUNIT_TEST(test_basic);

  CPPUNIT_TEST(test_open_error);
  CPPUNIT_TEST(test_open_sap);
  CPPUNIT_TEST(test_open_sap_error);
  CPPUNIT_TEST(test_open_flags);
  CPPUNIT_TEST(test_open_flags_error);

  CPPUNIT_TEST(test_open_port_single);
  CPPUNIT_TEST(test_open_port_single_error);
  CPPUNIT_TEST(test_open_port_range);
  CPPUNIT_TEST(test_open_port_range_error);

  CPPUNIT_TEST(test_open_sequential);
  CPPUNIT_TEST(test_open_randomize);

  CPPUNIT_TEST(test_accept);

  CPPUNIT_TEST_SUITE_END();

public:
  void test_basic();

  void test_open_error();
  void test_open_sap();
  void test_open_sap_error();
  void test_open_flags();
  void test_open_flags_error();

  void test_open_port_single();
  void test_open_port_single_error();
  void test_open_port_range();
  void test_open_port_range_error();

  void test_open_sequential();
  void test_open_randomize();

  void test_accept();
};

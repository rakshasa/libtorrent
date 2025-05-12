#include "helpers/test_fixture.h"

class test_socket_listen : public test_fixture {
  CPPUNIT_TEST_SUITE(test_socket_listen);

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
  static void test_basic();

  static void test_open_error();
  static void test_open_sap();
  static void test_open_sap_error();
  static void test_open_flags();
  static void test_open_flags_error();

  static void test_open_port_single();
  static void test_open_port_single_error();
  static void test_open_port_range();
  static void test_open_port_range_error();

  static void test_open_sequential();
  static void test_open_randomize();

  static void test_accept();
};

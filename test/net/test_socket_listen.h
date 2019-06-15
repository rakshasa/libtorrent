#include "helpers/test_fixture.h"

class test_socket_listen : public test_fixture {
  CPPUNIT_TEST_SUITE(test_socket_listen);

  CPPUNIT_TEST(test_basic);

  CPPUNIT_TEST(test_open_sap);
  CPPUNIT_TEST(test_open_flags);
  CPPUNIT_TEST(test_open_port_single);

  CPPUNIT_TEST(test_open_error);
  CPPUNIT_TEST(test_open_error_sap);
  CPPUNIT_TEST(test_open_error_flags);
  CPPUNIT_TEST(test_open_error_port);

  CPPUNIT_TEST_SUITE_END();

public:
  void test_basic();

  void test_open_sap();
  void test_open_flags();
  void test_open_port_single();

  void test_open_error();
  void test_open_error_sap();
  void test_open_error_flags();
  void test_open_error_port();
};

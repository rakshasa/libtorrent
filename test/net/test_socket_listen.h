#include "helpers/test_fixture.h"

class test_socket_listen : public test_fixture {
  CPPUNIT_TEST_SUITE(test_socket_listen);

  CPPUNIT_TEST(test_basic);
  CPPUNIT_TEST(test_open);

  CPPUNIT_TEST_SUITE_END();

public:
  void test_basic();
  void test_open();
};

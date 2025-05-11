#import "helpers/test_fixture.h"

class test_http : public test_fixture {
  CPPUNIT_TEST_SUITE(test_http);

  CPPUNIT_TEST(test_basic);
  CPPUNIT_TEST(test_done);
  CPPUNIT_TEST(test_failure);

  CPPUNIT_TEST(test_delete_on_done);
  CPPUNIT_TEST(test_delete_on_failure);

  CPPUNIT_TEST_SUITE_END();

public:
  static void test_basic();
  static void test_done();
  static void test_failure();

  static void test_delete_on_done();
  static void test_delete_on_failure();
};


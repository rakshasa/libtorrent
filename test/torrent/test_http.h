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
  void test_basic();
  void test_done();
  void test_failure();

  void test_delete_on_done();
  void test_delete_on_failure();
};


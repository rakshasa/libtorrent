#include <cppunit/extensions/HelperMacros.h>

#include "torrent/http.h"

class HttpTest : public CppUnit::TestFixture {
  CPPUNIT_TEST_SUITE(HttpTest);
  CPPUNIT_TEST(test_basic);
  CPPUNIT_TEST(test_done);
  CPPUNIT_TEST(test_failure);

  CPPUNIT_TEST(test_delete_on_done);
  CPPUNIT_TEST(test_delete_on_failure);
  CPPUNIT_TEST_SUITE_END();

public:
  void setUp() {}
  void tearDown() {}

  void test_basic();
  void test_done();
  void test_failure();

  void test_delete_on_done();
  void test_delete_on_failure();
};


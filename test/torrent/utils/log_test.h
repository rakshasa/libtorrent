#include <cppunit/extensions/HelperMacros.h>

#include "torrent/utils/log.h"

class utils_log_test : public CppUnit::TestFixture {
  CPPUNIT_TEST_SUITE(utils_log_test);
  CPPUNIT_TEST(test_basic);
  CPPUNIT_TEST(test_output_open);

  CPPUNIT_TEST(test_print);
  CPPUNIT_TEST(test_children);
  CPPUNIT_TEST(test_file_output);
  CPPUNIT_TEST_SUITE_END();

public:
  void setUp();
  void tearDown();

  void test_basic();
  void test_output_open();

  void test_print();
  void test_children();
  void test_file_output();
};

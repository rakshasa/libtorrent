#include "helpers/test_fixture.h"

class test_download_wrapper : public test_fixture {
  CPPUNIT_TEST_SUITE(test_download_wrapper);

  CPPUNIT_TEST(test_seek_window);
  CPPUNIT_TEST(test_seek_window_clear);
  CPPUNIT_TEST(test_seek_window_cap);
  CPPUNIT_TEST(test_promote_seek_window);
  CPPUNIT_TEST(test_promote_seek_window_off);

  CPPUNIT_TEST_SUITE_END();

public:
  void test_seek_window();
  void test_seek_window_clear();
  void test_seek_window_cap();
  void test_promote_seek_window();
  void test_promote_seek_window_off();
};

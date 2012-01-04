#include <cppunit/extensions/HelperMacros.h>

#include "torrent/utils/thread_base.h"

class utils_thread_base_test : public CppUnit::TestFixture {
  CPPUNIT_TEST_SUITE(utils_thread_base_test);
  CPPUNIT_TEST(test_basic);
  CPPUNIT_TEST(test_lifecycle);

  CPPUNIT_TEST(test_global_lock_basic);
  CPPUNIT_TEST_SUITE_END();

public:
  void setUp();
  void tearDown();

  void test_basic();
  void test_lifecycle();

  void test_global_lock_basic();
};

bool wait_for_true(std::tr1::function<bool ()> test_function);

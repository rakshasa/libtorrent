#include <cppunit/extensions/HelperMacros.h>

#include "torrent/utils/thread_base.h"

class utils_thread_base_test : public CppUnit::TestFixture {
  CPPUNIT_TEST_SUITE(utils_thread_base_test);
  CPPUNIT_TEST(test_basic);
  CPPUNIT_TEST(test_lifecycle);

  CPPUNIT_TEST(test_global_lock_basic);
  CPPUNIT_TEST(test_interrupt);
  CPPUNIT_TEST_SUITE_END();

public:
  void setUp();
  void tearDown();

  void test_basic();
  void test_lifecycle();

  void test_global_lock_basic();
  void test_interrupt();
};

#define SETUP_THREAD()                                                  \
  torrent::thread_disk* thread_disk = new torrent::thread_disk();       \
  torrent::thread_base::acquire_global_lock();                          \
  thread_disk->init_thread();

#define CLEANUP_THREAD()                                                \
  CPPUNIT_ASSERT(wait_for_true(tr1::bind(&torrent::thread_base::is_inactive, thread_disk))); \
  torrent::thread_base::release_global_lock();                          \
  delete thread_disk;

bool wait_for_true(std::tr1::function<bool ()> test_function);

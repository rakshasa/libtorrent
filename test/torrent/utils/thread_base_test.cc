#include "config.h"

#include <tr1/functional>
#include <torrent/utils/thread_base.h>

#include "thread_base_test.h"

CPPUNIT_TEST_SUITE_REGISTRATION(utils_thread_base_test);

namespace std { using namespace tr1; }

void
utils_thread_base_test::setUp() {
}

void
utils_thread_base_test::tearDown() {
}

void
utils_thread_base_test::test_basic() {
  torrent::thread_base* thread_base = new torrent::thread_base;

  CPPUNIT_ASSERT(!thread_base->is_main_polling());
  CPPUNIT_ASSERT(!thread_base->is_active());
  CPPUNIT_ASSERT(thread_base->global_queue_size() == 0);
  CPPUNIT_ASSERT(thread_base->poll() == NULL);

  // Check active...
}

// void
// utils_thread_base_test::test_global_lock...() {
// }

#include "config.h"

#include <torrent/exceptions.h>
#include <torrent/utils/signal_bitfield.h>
#include <torrent/utils/thread_base.h>

#include "signal_bitfield_test.h"
#include "thread_base_test.h"

CPPUNIT_TEST_SUITE_REGISTRATION(utils_signal_bitfield_test);

namespace tr1 { using namespace std::tr1; }

static void
mark_index(uint32_t* bitfield, unsigned int index) {
  __sync_fetch_and_or(bitfield, 1 << index);
}

static bool
check_index(uint32_t* bitfield, unsigned int index) {
  return *bitfield & (1 << index);
}

void
utils_signal_bitfield_test::setUp() {
}

void
utils_signal_bitfield_test::tearDown() {
  CPPUNIT_ASSERT(torrent::thread_base::trylock_global_lock());
  torrent::thread_base::release_global_lock();
}

static bool
verify_did_internal_error(tr1::function<void ()> func, bool should_throw) {
  bool did_throw = false;

  try {
    func();
  } catch (torrent::internal_error& e) {
    did_throw = true;
  }

  return should_throw == did_throw;
}

#define SETUP_SIGNAL_BITFIELD()                 \
  uint32_t marked_bitfield = 0;                 \
  torrent::signal_bitfield signal_bitfield;


#define SIGNAL_BITFIELD_DID_INTERNAL_ERROR(verify_slot, did_throw)      \
  CPPUNIT_ASSERT(verify_did_internal_error(tr1::bind(&torrent::signal_bitfield::add_signal, \
                                                     &signal_bitfield,  \
                                                     torrent::signal_bitfield::slot_type(verify_slot)), \
                                           did_throw));

void
utils_signal_bitfield_test::test_basic() {
  SETUP_SIGNAL_BITFIELD();

  CPPUNIT_ASSERT(torrent::signal_bitfield::max_size == sizeof(torrent::signal_bitfield::bitfield_type) * 8);

  SIGNAL_BITFIELD_DID_INTERNAL_ERROR(torrent::signal_bitfield::slot_type(), true);

  for (unsigned int i = 0; i < torrent::signal_bitfield::max_size; i++)
    CPPUNIT_ASSERT(signal_bitfield.add_signal(tr1::bind(&mark_index, &marked_bitfield, i)) == i);

  SIGNAL_BITFIELD_DID_INTERNAL_ERROR(tr1::bind(&mark_index, &marked_bitfield, torrent::signal_bitfield::max_size), true);
}

void
utils_signal_bitfield_test::test_single() {
  SETUP_SIGNAL_BITFIELD();

  CPPUNIT_ASSERT(signal_bitfield.add_signal(tr1::bind(&mark_index, &marked_bitfield, 0)) == 0);

  signal_bitfield.signal(0);
  CPPUNIT_ASSERT(marked_bitfield == 0x0);
  
  signal_bitfield.work();
  CPPUNIT_ASSERT(marked_bitfield == 0x1);

  marked_bitfield = 0;

  signal_bitfield.work();
  CPPUNIT_ASSERT(marked_bitfield == 0x0);
}

void
utils_signal_bitfield_test::test_multiple() {
  SETUP_SIGNAL_BITFIELD();

  for (unsigned int i = 0; i < torrent::signal_bitfield::max_size; i++)
    CPPUNIT_ASSERT(signal_bitfield.add_signal(tr1::bind(&mark_index, &marked_bitfield, i)) == i);

  signal_bitfield.signal(2);
  signal_bitfield.signal(31);
  CPPUNIT_ASSERT(marked_bitfield == 0x0);
  
  signal_bitfield.work();
  CPPUNIT_ASSERT(marked_bitfield == (((unsigned int)1 << 2) | ((unsigned int)1 << 31)));

  marked_bitfield = 0;

  signal_bitfield.work();
  CPPUNIT_ASSERT(marked_bitfield == 0x0);
}

void
utils_signal_bitfield_test::test_thread() {
  uint32_t marked_bitfield = 0;
  thread_test* thread = new thread_test;
  // thread->set_test_flag(thread_test::test_flag_long_timeout);

  for (unsigned int i = 0; i < torrent::signal_bitfield::max_size; i++)
    CPPUNIT_ASSERT(thread->signal_bitfield()->add_signal(tr1::bind(&mark_index, &marked_bitfield, i)) == i);

  thread->init_thread();
  thread->start_thread();

  // Vary the various timeouts.

  for (int i = 0; i < 100; i++) {
    // thread->interrupt();
    // usleep(0);

    thread->signal_bitfield()->signal(i % 20);
    // thread->interrupt();

    CPPUNIT_ASSERT(wait_for_true(tr1::bind(&check_index, &marked_bitfield, i % 20)));
    __sync_fetch_and_and(&marked_bitfield, ~uint32_t());
  }

  thread->stop_thread();
  CPPUNIT_ASSERT(wait_for_true(tr1::bind(&thread_test::is_state, thread, thread_test::STATE_INACTIVE)));

  delete thread;
}

// Test invalid signal added.
// Test overflow signals added.
// Test multiple signals triggered.

// Stresstest with real thread/polling.


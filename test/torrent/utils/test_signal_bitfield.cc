#include "config.h"

#include "test_signal_bitfield.h"

#include "helpers/test_thread.h"
#include "helpers/test_utils.h"

#include "torrent/exceptions.h"
#include "torrent/utils/signal_bitfield.h"
#include "torrent/utils/thread.h"

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(test_signal_bitfield, "torrent/utils");

inline void
bitfield_mark_index(std::atomic_uint32_t& bitfield, unsigned int index) {
  bitfield |= 1 << index;
}

static bool
check_index(std::atomic_uint32_t& bitfield, unsigned int index) {
  return bitfield & (1 << index);
}

static bool
verify_did_internal_error(std::function<unsigned int ()> func, bool should_throw) {
  bool did_throw = false;

  try {
    func();
  } catch (const torrent::internal_error&) {
    did_throw = true;
  }

  return should_throw == did_throw;
}

#define SETUP_SIGNAL_BITFIELD()                 \
  std::atomic_uint32_t marked_bitfield{0};      \
  torrent::signal_bitfield signal_bitfield;


#define SIGNAL_BITFIELD_DID_INTERNAL_ERROR(verify_slot, did_throw)      \
  CPPUNIT_ASSERT(verify_did_internal_error(std::bind(&torrent::signal_bitfield::add_signal, \
                                                     &signal_bitfield,  \
                                                     torrent::signal_bitfield::slot_type(verify_slot)), \
                                           did_throw));

void
test_signal_bitfield::test_basic() {
  SETUP_SIGNAL_BITFIELD();

  CPPUNIT_ASSERT(torrent::signal_bitfield::max_size == sizeof(torrent::signal_bitfield::bitfield_type) * 8);

  SIGNAL_BITFIELD_DID_INTERNAL_ERROR(torrent::signal_bitfield::slot_type(), true);

  for (unsigned int i = 0; i < torrent::signal_bitfield::max_size; i++)
    CPPUNIT_ASSERT(signal_bitfield.add_signal([i, &marked_bitfield] () { bitfield_mark_index(marked_bitfield, i); }) == i);

  SIGNAL_BITFIELD_DID_INTERNAL_ERROR([&marked_bitfield] () { bitfield_mark_index(marked_bitfield, torrent::signal_bitfield::max_size); }, true);
}

void
test_signal_bitfield::test_single() {
  SETUP_SIGNAL_BITFIELD();

  CPPUNIT_ASSERT(signal_bitfield.add_signal([&marked_bitfield] () { bitfield_mark_index(marked_bitfield, 0); }) == 0);

  signal_bitfield.signal(0);
  CPPUNIT_ASSERT(marked_bitfield == 0x0);

  signal_bitfield.work();
  CPPUNIT_ASSERT(marked_bitfield == 0x1);

  marked_bitfield = 0;

  signal_bitfield.work();
  CPPUNIT_ASSERT(marked_bitfield == 0x0);
}

void
test_signal_bitfield::test_multiple() {
  SETUP_SIGNAL_BITFIELD();

  for (unsigned int i = 0; i < torrent::signal_bitfield::max_size; i++)
    CPPUNIT_ASSERT(signal_bitfield.add_signal([i, &marked_bitfield] () { bitfield_mark_index(marked_bitfield, i); }) == i);

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
test_signal_bitfield::test_threaded() {
  std::atomic_uint32_t marked_bitfield{0};

  auto thread = test_thread::create();
  // thread->set_test_flag(test_thread::test_flag_long_timeout);

  for (unsigned int i = 0; i < torrent::signal_bitfield::max_size; i++)
    CPPUNIT_ASSERT(thread->signal_bitfield()->add_signal([i, &marked_bitfield] () { bitfield_mark_index(marked_bitfield, i); }) == i);

  thread->init_thread();
  thread->start_thread();

  // Vary the various timeouts.

  for (int i = 0; i < 100; i++) {
    // thread->interrupt();
    // usleep(0);

    thread->signal_bitfield()->signal(i % 20);
    // thread->interrupt();

    CPPUNIT_ASSERT(wait_for_true([i, &marked_bitfield] () { return check_index(marked_bitfield, i % 20); }));
    marked_bitfield &= ~uint32_t();
  }

  thread->stop_thread_wait();
  CPPUNIT_ASSERT(wait_for_true(std::bind(&test_thread::is_state, thread.get(), test_thread::STATE_INACTIVE)));
}

// Test invalid signal added.
// Test overflow signals added.
// Test multiple signals triggered.

// Stresstest with real thread/polling.


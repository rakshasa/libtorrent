#include "config.h"

#include <torrent/exceptions.h>
#include <torrent/utils/signal_bitfield.h>
#include <torrent/utils/thread_base.h>

#include "signal_bitfield_test.h"

CPPUNIT_TEST_SUITE_REGISTRATION(utils_signal_bitfield_test);

namespace tr1 { using namespace std::tr1; }

static void
mark_index(uint32_t* bitfield, unsigned int index) {
  __sync_fetch_and_or(bitfield, 1 << index);
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


void
utils_signal_bitfield_test::test_basic() {
  uint32_t marked_bitfield = 0;
  torrent::signal_bitfield signal_bitfield;

  CPPUNIT_ASSERT(torrent::signal_bitfield::max_size == sizeof(torrent::signal_bitfield::bitfield_type) * 8);

  for (unsigned int i = 0; i < torrent::signal_bitfield::max_size; i++)
    CPPUNIT_ASSERT(signal_bitfield.add_signal(tr1::bind(&mark_index, &marked_bitfield, i)) == i);

  CPPUNIT_ASSERT(verify_did_internal_error(tr1::bind(&torrent::signal_bitfield::add_signal,
                                                     &signal_bitfield,
                                                     torrent::signal_bitfield::slot_type(tr1::bind(&mark_index, &marked_bitfield, torrent::signal_bitfield::max_size))),
                                           true));
}

void
utils_signal_bitfield_test::test_single() {
  uint32_t marked_bitfield = 0;
  torrent::signal_bitfield signal_bitfield;

  CPPUNIT_ASSERT(signal_bitfield.add_signal(tr1::bind(&mark_index, &marked_bitfield, 0)) == 0);
}

// Test invalid signal added.
// Test overflow signals added.
// Test multiple signals triggered.

// Stresstest with real thread/polling.
// 

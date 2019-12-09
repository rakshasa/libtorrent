#import "config.h"

#import "test_thread.h"

#import <unistd.h>
#import <cppunit/extensions/HelperMacros.h>

#import "thread_disk.h"
#import "torrent/exceptions.h"
#import "torrent/poll_select.h"

const int test_thread::test_flag_pre_stop;
const int test_thread::test_flag_long_timeout;

const int test_thread::test_flag_acquire_global;
const int test_thread::test_flag_has_global;

const int test_thread::test_flag_do_work;
const int test_thread::test_flag_pre_poke;
const int test_thread::test_flag_post_poke;

test_thread::test_thread() :
  m_test_state(TEST_NONE),
  m_test_flags(0) {
}

void
test_thread::init_thread() {
  m_state = STATE_INITIALIZED;
  m_test_state = TEST_PRE_START;
  m_poll = torrent::PollSelect::create(256);
}

void
test_thread::call_events() {
  if ((m_test_flags & test_flag_pre_stop) && m_test_state == TEST_PRE_START && m_state == STATE_ACTIVE)
    __sync_lock_test_and_set(&m_test_state, TEST_PRE_STOP);

  if ((m_test_flags & test_flag_acquire_global)) {
    acquire_global_lock();
    __sync_and_and_fetch(&m_test_flags, ~test_flag_acquire_global);
    __sync_or_and_fetch(&m_test_flags, test_flag_has_global);
  }

  if ((m_flags & flag_do_shutdown)) {
    if ((m_flags & flag_did_shutdown))
      throw torrent::internal_error("Already trigged shutdown.");

    __sync_or_and_fetch(&m_flags, flag_did_shutdown);
    throw torrent::shutdown_exception();
  }

  if ((m_test_flags & test_flag_pre_poke)) {
  }

  if ((m_test_flags & test_flag_do_work)) {
    usleep(10 * 1000); // TODO: Don't just sleep, as that give up core.
    __sync_and_and_fetch(&m_test_flags, ~test_flag_do_work);
  }

  if ((m_test_flags & test_flag_post_poke)) {
  }
}

thread_management_type::thread_management_type() {
  CPPUNIT_ASSERT(torrent::thread_base::trylock_global_lock());
}

thread_management_type::~thread_management_type() {
  torrent::thread_base::release_global_lock();
}

#include "config.h"

#include "test_thread.h"

#include <unistd.h>
#include <cppunit/extensions/HelperMacros.h>

#include "data/thread_disk.h"
#include "torrent/exceptions.h"
#include "torrent/poll_select.h"

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
  // m_thread_id = std::this_thread::get_id();

  m_poll = std::unique_ptr<torrent::PollSelect>(torrent::PollSelect::create(256));
}

void
test_thread::call_events() {
  if ((m_test_flags & test_flag_pre_stop) && m_test_state == TEST_PRE_START && m_state == STATE_ACTIVE)
    m_test_state = TEST_PRE_STOP;

  if ((m_test_flags & test_flag_acquire_global)) {
    acquire_global_lock();
    m_test_flags &= ~test_flag_acquire_global;
    m_test_flags |= test_flag_has_global;
  }

  if ((m_flags & flag_do_shutdown)) {
    if ((m_flags & flag_did_shutdown))
      throw torrent::internal_error("Already trigged shutdown.");

    m_flags |= flag_did_shutdown;
    throw torrent::shutdown_exception();
  }

  if ((m_test_flags & test_flag_pre_poke)) {
  }

  if ((m_test_flags & test_flag_do_work)) {
    usleep(10 * 1000); // TODO: Don't just sleep, as that give up core.
    m_test_flags &= ~test_flag_do_work;
  }

  if ((m_test_flags & test_flag_post_poke)) {
  }

  // while (!taskScheduler.empty() && taskScheduler.top()->time() <= cachedTime) {
  //   rak::priority_item* v = taskScheduler.top();
  //   taskScheduler.pop();

  //   v->clear_time();
  //   v->slot()();
  // }

  process_callbacks();
}

thread_management_type::thread_management_type() {
  CPPUNIT_ASSERT(torrent::utils::Thread::trylock_global_lock());
}

thread_management_type::~thread_management_type() {
  torrent::utils::Thread::release_global_lock();
}

void
set_create_poll() {
  torrent::Poll::slot_create_poll() = [] {
      return torrent::PollSelect::create(256);
    };
}

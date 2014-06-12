// libTorrent - BitTorrent library
// Copyright (C) 2005-2011, Jari Sundell
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
// In addition, as a special exception, the copyright holders give
// permission to link the code of portions of this program with the
// OpenSSL library under certain conditions as described in each
// individual source file, and distribute linked combinations
// including the two.
//
// You must obey the GNU General Public License in all respects for
// all of the code used other than OpenSSL.  If you modify file(s)
// with this exception, you may extend this exception to your version
// of the file(s), but you are not obligated to do so.  If you do not
// wish to do so, delete this exception statement from your version.
// If you delete this exception statement from all source files in the
// program, then also delete it here.
//
// Contact:  Jari Sundell <jaris@ifi.uio.no>
//
//           Skomakerveien 33
//           3185 Skoppum, NORWAY

#include "config.h"

#include <cstring>
#include <signal.h>
#include <unistd.h>

#include "exceptions.h"
#include "poll.h"
#include "thread_base.h"
#include "thread_interrupt.h"
#include "utils/log.h"
#include "utils/instrumentation.h"

namespace torrent {

thread_base::global_lock_type lt_cacheline_aligned thread_base::m_global = { 0, 0, PTHREAD_MUTEX_INITIALIZER };

thread_base::thread_base() :
  m_state(STATE_UNKNOWN),
  m_flags(0),
  m_instrumentation_index(INSTRUMENTATION_POLLING_DO_POLL_OTHERS - INSTRUMENTATION_POLLING_DO_POLL),

  m_poll(NULL),
  m_interrupt_sender(NULL),
  m_interrupt_receiver(NULL)
{
  std::memset(&m_thread, 0, sizeof(pthread_t));

// #ifdef USE_INTERRUPT_SOCKET
  thread_interrupt::pair_type interrupt_sockets = thread_interrupt::create_pair();

  m_interrupt_sender = interrupt_sockets.first;
  m_interrupt_receiver = interrupt_sockets.second;
// #endif
}

thread_base::~thread_base() {
  delete m_interrupt_sender;
  delete m_interrupt_receiver;
}

void
thread_base::start_thread() {
  if (m_poll == NULL)
    throw internal_error("No poll object for thread defined.");

  if (!is_initialized() || pthread_create(&m_thread, NULL, (pthread_func)&thread_base::event_loop, this))
    throw internal_error("Failed to create thread.");
}

void
thread_base::stop_thread() {
  __sync_fetch_and_or(&m_flags, flag_do_shutdown);
  interrupt();
}

void
thread_base::stop_thread_wait() {
  stop_thread();

  release_global_lock();

  while (!is_inactive()) {
    usleep(1000);
  }  

  acquire_global_lock();
}

// Fix interrupting when shutting down thread.
void
thread_base::interrupt() {
  // Only poke when polling, set no_timeout
  if (is_polling())
    m_interrupt_sender->poke();
}

bool
thread_base::should_handle_sigusr1() {
  return false;
}

void*
thread_base::event_loop(thread_base* thread) {
  __sync_lock_test_and_set(&thread->m_state, STATE_ACTIVE);

#if defined(HAS_PTHREAD_SETNAME_NP_DARWIN)
  pthread_setname_np(thread->name());
#elif defined(HAS_PTHREAD_SETNAME_NP_GENERIC)
  pthread_setname_np(thread->m_thread, thread->name());
#endif

  lt_log_print(torrent::LOG_THREAD_NOTICE, "%s: Starting thread.", thread->name());
  
  try {

// #ifdef USE_INTERRUPT_SOCKET
    thread->m_poll->insert_read(thread->m_interrupt_receiver);
// #endif

    while (true) {
      if (thread->m_slot_do_work)
        thread->m_slot_do_work();

      thread->call_events();
      thread->signal_bitfield()->work();

      __sync_fetch_and_or(&thread->m_flags, flag_polling);

      // Call again after setting flag_polling to ensure we process
      // any events set while it was working.
      if (thread->m_slot_do_work)
        thread->m_slot_do_work();

      thread->call_events();
      thread->signal_bitfield()->work();

      uint64_t next_timeout = 0;

      if (!thread->has_no_timeout()) {
        next_timeout = thread->next_timeout_usec();

        if (thread->m_slot_next_timeout)
          next_timeout = std::min(next_timeout, thread->m_slot_next_timeout());
      }

      // Add the sleep call when testing interrupts, etc.
      // usleep(50);

      int poll_flags = 0;

      if (!(thread->flags() & flag_main_thread))
        poll_flags = torrent::Poll::poll_worker_thread;

      instrumentation_update(INSTRUMENTATION_POLLING_DO_POLL, 1);
      instrumentation_update(instrumentation_enum(INSTRUMENTATION_POLLING_DO_POLL + thread->m_instrumentation_index), 1);

      int event_count = thread->m_poll->do_poll(next_timeout, poll_flags);

      instrumentation_update(INSTRUMENTATION_POLLING_EVENTS, event_count);
      instrumentation_update(instrumentation_enum(INSTRUMENTATION_POLLING_EVENTS + thread->m_instrumentation_index), event_count);

      __sync_fetch_and_and(&thread->m_flags, ~(flag_polling | flag_no_timeout));
    }

// #ifdef USE_INTERRUPT_SOCKET
    thread->m_poll->remove_write(thread->m_interrupt_receiver);
// #endif

  } catch (torrent::shutdown_exception& e) {
    lt_log_print(torrent::LOG_THREAD_NOTICE, "%s: Shutting down thread.", thread->name());
  }

  __sync_lock_test_and_set(&thread->m_state, STATE_INACTIVE);
  return NULL;
}

}

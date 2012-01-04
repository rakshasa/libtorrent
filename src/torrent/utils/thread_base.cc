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

#include "exceptions.h"
#include "poll.h"
#include "thread_base.h"
#include "utils/log.h"

namespace torrent {

thread_base::global_lock_type lt_cacheline_aligned thread_base::m_global = { 0, 0, PTHREAD_MUTEX_INITIALIZER };

thread_base::thread_base() :
  m_state(STATE_UNKNOWN),
  m_flags(0),
  m_poll(NULL)
{
  std::memset(&m_thread, 0, sizeof(pthread_t));
}

void
thread_base::start_thread() {
  if (m_poll == NULL)
    throw internal_error("No poll object for thread defined.");

  if (m_state != STATE_INITIALIZED ||
      pthread_create(&m_thread, NULL, (pthread_func)&thread_base::event_loop, this))
    throw internal_error("Failed to create thread.");
}

void
thread_base::stop_thread() {
  __sync_fetch_and_or(&m_flags, flag_do_shutdown);
  interrupt();
}

void*
thread_base::event_loop(thread_base* thread) {
  __sync_lock_test_and_set(&thread->m_state, STATE_ACTIVE);
  lt_log_print_locked(torrent::LOG_THREAD_NOTICE, "Starting thread.");
  
  try {

    while (true) {
      thread->call_events();
      thread->m_poll->do_poll(thread->next_timeout_usec(), torrent::Poll::poll_worker_thread);
    }

  } catch (torrent::shutdown_exception& e) {
    lt_log_print_locked(torrent::LOG_THREAD_NOTICE, "Shutting down thread.");
  }

  __sync_lock_test_and_set(&thread->m_state, STATE_INACTIVE);
  return NULL;
}

}

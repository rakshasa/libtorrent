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

#include <rak/timer.h>

#include "thread_main.h"

#include "globals.h"
#include "torrent/exceptions.h"
#include "torrent/poll.h"
#include "torrent/utils/log.h"
#include "utils/instrumentation.h"

namespace torrent {

void
thread_main::init_thread() {
  acquire_global_lock();

  if (!Poll::slot_create_poll())
    throw internal_error("thread_main::init_thread(): Poll::slot_create_poll() not valid.");

  m_poll = Poll::slot_create_poll()();
  m_poll->set_flags(Poll::flag_waive_global_lock);

  m_state = STATE_INITIALIZED;
  m_thread = pthread_self();
  m_flags |= flag_main_thread;

  m_instrumentation_index = INSTRUMENTATION_POLLING_DO_POLL_MAIN - INSTRUMENTATION_POLLING_DO_POLL;
}

void
thread_main::call_events() {
  cachedTime = rak::timer::current();

  // Ensure we don't call rak::timer::current() twice if there was no
  // scheduled tasks called.
  if (taskScheduler.empty() || taskScheduler.top()->time() > cachedTime)
    return;

  while (!taskScheduler.empty() && taskScheduler.top()->time() <= cachedTime) {
    rak::priority_item* v = taskScheduler.top();
    taskScheduler.pop();

    v->clear_time();
    v->slot()();
  }

  // Update the timer again to ensure we get accurate triggering of
  // msec timers.
  cachedTime = rak::timer::current();
}

int64_t
thread_main::next_timeout_usec() {
  cachedTime = rak::timer::current();

  if (!taskScheduler.empty())
    return std::max(taskScheduler.top()->time() - cachedTime, rak::timer()).usec();
  else
    return rak::timer::from_seconds(60).usec();
}

}

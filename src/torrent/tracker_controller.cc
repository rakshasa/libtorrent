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

#include "download_info.h"
#include "tracker_controller.h"
#include "tracker_list.h"

#include "rak/priority_queue_default.h"
#include "utils/log.h"
#include "tracker/tracker_dht.h"
#include "tracker/tracker_http.h"
#include "tracker/tracker_manager.h"
#include "tracker/tracker_udp.h"

#include "globals.h"

#define LT_LOG_TRACKER(log_level, log_fmt, ...)                         \
  lt_log_print_info(LOG_TRACKER_##log_level, m_tracker_list->info(), "->tracker_controller: " log_fmt, __VA_ARGS__);

namespace torrent {

struct tracker_controller_private {
  rak::priority_item task_timeout;
};

// End temp hacks...

TrackerController::TrackerController(TrackerList* trackers) :
  m_flags(0),
  m_tracker_list(trackers),
  m_failed_requests(0),
  m_private(new tracker_controller_private) {

  m_private->task_timeout.set_slot(rak::mem_fn(this, &TrackerController::receive_timeout));
}

TrackerController::~TrackerController() {
  priority_queue_erase(&taskScheduler, &m_private->task_timeout);
  delete m_private;
}

rak::priority_item*
TrackerController::task_timeout() {
  return &m_private->task_timeout;
}

int64_t
TrackerController::next_timeout() const {
  return m_private->task_timeout.time().usec();
}

uint32_t
TrackerController::seconds_to_next_timeout() const {
  return std::max(m_private->task_timeout.time() - cachedTime, rak::timer()).seconds_ceiling();
}

// TODO: Move to tracker_list?
void
TrackerController::insert(int group, const std::string& url) {
  if (m_tracker_list->has_active())
    throw internal_error("Tried to add tracker while a tracker is active.");

  Tracker* tracker;

  if (std::strncmp("http://", url.c_str(), 7) == 0 ||
      std::strncmp("https://", url.c_str(), 8) == 0) {
    tracker = new TrackerHttp(m_tracker_list, url);

  } else if (std::strncmp("udp://", url.c_str(), 6) == 0) {
    tracker = new TrackerUdp(m_tracker_list, url);

  } else if (std::strncmp("dht://", url.c_str(), 6) == 0 && TrackerDht::is_allowed()) {
    tracker = new TrackerDht(m_tracker_list, url);

  } else {
    LT_LOG_TRACKER(WARN, "Could find matching tracker protocol for url:'%s'.", url.c_str());
    return;
  }
  
  LT_LOG_TRACKER(INFO, "Added tracker group:%i url:'%s'.", group, url.c_str());

  m_tracker_list->insert(group, tracker);
}

// The send_*_event() functions tries to ensure the relevant trackers
// receive the event.
//
// When we just want more peers the start_requesting() function is
// used. This is all independent of the regular updates sent to the
// trackers.

void
TrackerController::send_start_event() {
  // This will now be 'lazy', rather than a definite event. We tell
  // the controller that a 'start' event should be sent, and it will
  // send it when the tracker controller get's enabled.

  // If the controller is already running, we insert this new event.
  
  // Return, or something, if already active and sending?

  if (m_flags & flag_send_start) {
    // Do we just return, or bork? At least we need to check to see
    // that there's something requesting 'start' event or fail hard.
  }

  m_flags &= ~mask_send;
  m_flags |= flag_send_start;
  
  if (m_flags & flag_active && m_tracker_list->has_usable()) {
    // Start with requesting from the first tracker. Add timer to
    // catch when we don't get any response within the first few
    // seconds, at which point we go promiscious.

    // Do we use the old 'focus' thing?... Rather react on no reply,
    // go into promiscious.

    // Do we really want to close all here? Probably, as we're sending
    // a new event.
    LT_LOG_TRACKER(INFO, "Sending started event.", 0);

    close();
    m_tracker_list->send_state_tracker(m_tracker_list->find_usable(m_tracker_list->begin()), Tracker::EVENT_STARTED);

  } else {
    LT_LOG_TRACKER(INFO, "Queueing started event.", 0);
  }
}

void
TrackerController::send_stop_event() {
  if (m_flags & flag_send_stop) {
    // Do we just return, or bork? At least we need to check to see
    // that there's something requesting 'start' event or fail hard.
  }

  m_flags &= ~mask_send;
  m_flags |= flag_send_stop;
  
  if (m_flags & flag_active && m_tracker_list->has_usable()) {
    LT_LOG_TRACKER(INFO, "Sending stopped event.", 0);

    close();
    TrackerList::iterator itr = std::find_if(m_tracker_list->begin(), m_tracker_list->end(), std::mem_fun(&Tracker::is_in_use));

    while (itr != m_tracker_list->end()) {
      m_tracker_list->send_state_tracker(itr, Tracker::EVENT_STOPPED);
      itr = std::find_if(itr + 1, m_tracker_list->end(), std::mem_fun(&Tracker::is_in_use));
    }

    // Timer...

  } else {
    LT_LOG_TRACKER(INFO, "Queueing stopped event.", 0);
  }
}

void
TrackerController::send_completed_event() {
  if (m_flags & flag_send_completed) {
    // Do we just return, or bork? At least we need to check to see
    // that there's something requesting 'start' event or fail hard.
  }

  m_flags &= ~mask_send;
  m_flags |= flag_send_completed;
  
  if (m_flags & flag_active && m_tracker_list->has_usable()) {
    LT_LOG_TRACKER(INFO, "Sending completed event.", 0);

    // Send to all trackers that would want to know.

    close();
    TrackerList::iterator itr = std::find_if(m_tracker_list->begin(), m_tracker_list->end(), std::mem_fun(&Tracker::is_in_use));

    while (itr != m_tracker_list->end()) {
      m_tracker_list->send_state_tracker(itr, Tracker::EVENT_COMPLETED);
      itr = std::find_if(itr + 1, m_tracker_list->end(), std::mem_fun(&Tracker::is_in_use));
    }

    // Timer...

  } else {
    LT_LOG_TRACKER(INFO, "Queueing completed event.", 0);
  }
}

// Currently being used by send_state, fixme.
void
TrackerController::close() {
  m_failed_requests = 0;

  stop_requesting();

  m_tracker_list->close_all();
  priority_queue_erase(&taskScheduler, &m_private->task_timeout);
}

void
TrackerController::enable() {
  if ((m_flags & flag_active))
    return;

  m_flags |= flag_active;

  LT_LOG_TRACKER(INFO, "Called enable with %u trackers.", m_tracker_list->size());

  // Adding of the tracker requests gets done after the caller has had
  // a chance to override the default behavior.
  priority_queue_insert(&taskScheduler, &m_private->task_timeout, cachedTime);
}

void
TrackerController::disable() {
  if (!(m_flags & flag_active))
    return;

  // Disable other flags?...
  m_flags &= ~flag_active;

  close();
  priority_queue_erase(&taskScheduler, &m_private->task_timeout);

  LT_LOG_TRACKER(INFO, "Called disable with %u trackers.", m_tracker_list->size());
}

void
TrackerController::start_requesting() {
  m_flags |= flag_requesting;

  priority_queue_erase(&taskScheduler, &m_private->task_timeout);
  priority_queue_insert(&taskScheduler, &m_private->task_timeout, cachedTime);
}

void
TrackerController::stop_requesting() {
  m_flags &= ~flag_requesting;

  // Should check if timeout is set?
  if (!m_tracker_list->has_active() && m_tracker_list->has_usable()) {
    priority_queue_erase(&taskScheduler, &m_private->task_timeout);
    priority_queue_insert(&taskScheduler, &m_private->task_timeout,
                          // Improve this...
                          cachedTime + rak::timer::from_seconds((*m_tracker_list->find_usable(m_tracker_list->begin()))->normal_interval()).round_seconds());
  }
}

void
TrackerController::receive_timeout() {
  if (!(m_flags & flag_active) || !m_tracker_list->has_usable())
    return;

  //  LT_LOG_TRACKER(INFO, "Timeout called.");

  // Handle the different states properly...

  // Do we want the timeout function to know what tracker we queued
  // the timeout for? Seems reasonable, as that allows us to do
  // iteration through multiple trackers.

  int send_state;

  if ((m_flags & flag_send_start)) {
    send_state = Tracker::EVENT_STARTED;
  } else if ((m_flags & flag_send_stop)) {
    send_state = Tracker::EVENT_STOPPED;;
  } else if ((m_flags & flag_send_completed)) {
    send_state = Tracker::EVENT_COMPLETED;
  } else {
    send_state = Tracker::EVENT_NONE;
  }

  // Find the next tracker to try...
  TrackerList::iterator itr = m_tracker_list->find_usable(m_tracker_list->begin());

  if (itr != m_tracker_list->end()) {
    TrackerList::iterator preferred = itr;

    while (++itr != m_tracker_list->end()) {
      if (!(*itr)->is_usable())
        continue;

      if ((*itr)->failed_counter() <= (*preferred)->failed_counter() &&
          (*itr)->failed_time_last() < (*preferred)->failed_time_last())
        preferred = itr;
    }

    m_tracker_list->send_state_tracker(preferred, send_state);
  }

  if (m_slot_timeout)
    m_slot_timeout();
}

void
TrackerController::receive_success_new(Tracker* tb, TrackerController::address_list* l) {
  m_failed_requests = 0;

  // if (<check if we have multiple trackers to send this event to, before we declare success>) {
  m_flags &= ~mask_send;
  // }

  //unsigned int next_request = m_tracker_list->focus_normal_interval();

  // If we still have active trackers, skip the timeout.

  // Calculate the next timeout according to a list of in-use
  // trackers, with the first timeout as the interval.

  // For the moment, just use the last tracker...
  unsigned int next_request = tb->normal_interval();

  // Also make this check how many new peers we got, and how often
  // we've reconnected this time.
  if (m_flags & flag_requesting)
    next_request = 30;

  priority_queue_erase(&taskScheduler, &m_private->task_timeout);
  priority_queue_insert(&taskScheduler, &m_private->task_timeout,
                        (cachedTime + rak::timer::from_seconds(next_request)).round_seconds());
  
  m_slot_success(l);
}

void
TrackerController::receive_failure_new(Tracker* tb, const std::string& msg) {
  if (tb == NULL) {
    LT_LOG_TRACKER(INFO, "Received failure msg:'%s'.", msg.c_str());
    m_slot_failure(msg);
    return;
  }

  // For the moment, just use the last tracker...
  unsigned int next_request = 5 << std::min<int>(tb->failed_counter() - 1, 6);

  priority_queue_erase(&taskScheduler, &m_private->task_timeout);
  priority_queue_insert(&taskScheduler, &m_private->task_timeout,
                        (cachedTime + rak::timer::from_seconds(next_request)).round_seconds());

  m_flags |= flag_failure_mode;

  m_slot_failure(msg);
}

}

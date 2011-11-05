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
#include "tracker/tracker_udp.h"

#include "globals.h"

#define LT_LOG_TRACKER(log_level, log_fmt, ...)                         \
  lt_log_print_info(LOG_TRACKER_##log_level, m_tracker_list->info(), "->tracker_controller: " log_fmt, __VA_ARGS__);

namespace torrent {

struct tracker_controller_private {
  rak::priority_item task_timeout;
};

// End temp hacks...

void
TrackerController::update_timeout(uint32_t seconds_to_next) {
  if (!(m_flags & flag_active))
    throw internal_error("TrackerController cannot set timeout when inactive.");

  rak::timer next_timeout = cachedTime;

  if (seconds_to_next != 0)
    next_timeout = (cachedTime + rak::timer::from_seconds(seconds_to_next)).round_seconds();

  priority_queue_erase(&taskScheduler, &m_private->task_timeout);
  priority_queue_insert(&taskScheduler, &m_private->task_timeout, next_timeout);
}

inline int
TrackerController::current_send_state() const {
  switch ((m_flags & mask_send)) {
  case flag_send_start:     return Tracker::EVENT_STARTED;
  case flag_send_stop:      return Tracker::EVENT_STOPPED;
  case flag_send_completed: return Tracker::EVENT_COMPLETED;
  default:                  return Tracker::EVENT_NONE;
  }
}

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
//
// TODO: Use proper flags for insert options.
void
TrackerController::insert(int group, const std::string& url, bool extra_tracker) {
  Tracker* tracker;
  int flags = Tracker::flag_enabled;

  if (extra_tracker)
    flags |= Tracker::flag_extra_tracker;

  if (std::strncmp("http://", url.c_str(), 7) == 0 ||
      std::strncmp("https://", url.c_str(), 8) == 0) {
    tracker = new TrackerHttp(m_tracker_list, url, flags);

  } else if (std::strncmp("udp://", url.c_str(), 6) == 0) {
    tracker = new TrackerUdp(m_tracker_list, url, flags);

  } else if (std::strncmp("dht://", url.c_str(), 6) == 0 && TrackerDht::is_allowed()) {
    tracker = new TrackerDht(m_tracker_list, url, flags);

  } else {
    LT_LOG_TRACKER(WARN, "Could find matching tracker protocol for url: '%s'.", url.c_str());

    if (extra_tracker)
      throw torrent::input_error("Could find matching tracker protocol for url: '" + url + "'.");

    return;
  }
  
  LT_LOG_TRACKER(INFO, "Added tracker group:%i url:'%s'.", group, url.c_str());

  m_tracker_list->insert(group, tracker);
}


void
TrackerController::manual_request(bool request_now) {
  if (!m_private->task_timeout.is_queued())
    return;

  // Add functions to get the lowest timeout, etc...

  // if (!force)
  //   t = std::max(t, rak::timer::from_seconds(m_tracker_list->time_last_connection() + m_tracker_list->focus_min_interval()));

  update_timeout(2);
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
  
  if (!(m_flags & flag_active) || !m_tracker_list->has_usable()) {
    LT_LOG_TRACKER(INFO, "Queueing started event.", 0);
    return;
  }

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

  if (m_tracker_list->count_usable() > 1) {
    m_flags |= flag_promiscuous_mode;
    update_timeout(3);
  }
}

void
TrackerController::send_stop_event() {
  if (m_flags & flag_send_stop) {
    // Do we just return, or bork? At least we need to check to see
    // that there's something requesting 'start' event or fail hard.
  }

  m_flags &= ~mask_send;

  if (!(m_flags & flag_active) || !m_tracker_list->has_usable()) {
    LT_LOG_TRACKER(INFO, "Skipping stopped event as no tracker need it.", 0);
    return;
  }

  m_flags |= flag_send_stop;
  
  LT_LOG_TRACKER(INFO, "Sending stopped event.", 0);

  close();
  TrackerList::iterator itr = std::find_if(m_tracker_list->begin(), m_tracker_list->end(), std::mem_fun(&Tracker::is_in_use));

  while (itr != m_tracker_list->end()) {
    m_tracker_list->send_state_tracker(itr, Tracker::EVENT_STOPPED);
    itr = std::find_if(itr + 1, m_tracker_list->end(), std::mem_fun(&Tracker::is_in_use));
  }

  // Timer...
}

void
TrackerController::send_completed_event() {
  if (m_flags & flag_send_completed) {
    // Do we just return, or bork? At least we need to check to see
    // that there's something requesting 'start' event or fail hard.
  }

  m_flags &= ~mask_send;
  m_flags |= flag_send_completed;
  
  if (!(m_flags & flag_active) || !m_tracker_list->has_usable()) {
    LT_LOG_TRACKER(INFO, "Queueing completed event.", 0);
    return;
  }

  LT_LOG_TRACKER(INFO, "Sending completed event.", 0);

  // Send to all trackers that would want to know.

  close();
  TrackerList::iterator itr = std::find_if(m_tracker_list->begin(), m_tracker_list->end(), std::mem_fun(&Tracker::is_in_use));

  while (itr != m_tracker_list->end()) {
    m_tracker_list->send_state_tracker(itr, Tracker::EVENT_COMPLETED);
    itr = std::find_if(itr + 1, m_tracker_list->end(), std::mem_fun(&Tracker::is_in_use));
  }

  // Timer...
}

void
TrackerController::send_update_event() {
  if (!(m_flags & flag_active) || !m_tracker_list->has_usable())
    return;

  if ((m_flags & mask_send) || m_tracker_list->has_active())
    return;

  LT_LOG_TRACKER(INFO, "Sending update event.", 0);

  m_tracker_list->send_state_tracker(m_tracker_list->find_usable(m_tracker_list->begin()), Tracker::EVENT_STARTED);

  if (m_tracker_list->has_active())
    priority_queue_erase(&taskScheduler, &m_private->task_timeout);
}

// Currently being used by send_state, fixme.
void
TrackerController::close() {
  m_failed_requests = 0;
  m_flags &= ~(flag_requesting | flag_promiscuous_mode);

  m_tracker_list->close_all();
  priority_queue_erase(&taskScheduler, &m_private->task_timeout);
}

void
TrackerController::enable() {
  if ((m_flags & flag_active))
    return;

  m_flags |= flag_active;

  m_tracker_list->close_all_excluding((1 << Tracker::EVENT_COMPLETED));
  LT_LOG_TRACKER(INFO, "Called enable with %u trackers.", m_tracker_list->size());

  // Adding of the tracker requests gets done after the caller has had
  // a chance to override the default behavior.
  update_timeout(0);
}

void
TrackerController::disable() {
  if (!(m_flags & flag_active))
    return;

  // Disable other flags?...
  m_flags &= ~(flag_active | flag_requesting | flag_requesting | flag_promiscuous_mode);

  m_tracker_list->close_all_excluding((1 << Tracker::EVENT_STOPPED) | (1 << Tracker::EVENT_COMPLETED));
  priority_queue_erase(&taskScheduler, &m_private->task_timeout);

  LT_LOG_TRACKER(INFO, "Called disable with %u trackers.", m_tracker_list->size());
}

void
TrackerController::start_requesting() {
  if (!(m_flags & flag_active) || (m_flags & flag_requesting))
    return;

  m_flags |= flag_requesting;

  update_timeout(0);
}

void
TrackerController::stop_requesting() {
  if (!(m_flags & flag_active) || !(m_flags & flag_requesting))
    return;

  m_flags &= ~flag_requesting;

  // Should check if timeout is set?
  if (!m_tracker_list->has_active() && m_tracker_list->has_usable())
    update_timeout((*m_tracker_list->find_usable(m_tracker_list->begin()))->normal_interval());
}

void
TrackerController::receive_timeout() {
  if (!(m_flags & flag_active) || !m_tracker_list->has_usable())
    return;

  // Handle the different states properly...

  // Do we want the timeout function to know what tracker we queued
  // the timeout for? Seems reasonable, as that allows us to do
  // iteration through multiple trackers.

  int send_state = current_send_state();
  TrackerList::iterator itr = m_tracker_list->find_usable(m_tracker_list->begin());

  if ((m_flags & flag_promiscuous_mode)) {
    // When in promiscious mode we want as many peers as quickly as
    // possible, so every available tracker is requested. The
    // promiscious mode only gets acted upon in the timeout handler so
    // as to give the primary tracker a few seconds to reply.
    for (; itr != m_tracker_list->end(); itr++) {
      if ((*itr)->is_busy() || !(*itr)->is_usable())
        continue;

      m_tracker_list->send_state_tracker(itr, send_state);
    }

  } else {
    // Find the next tracker to try...
    TrackerList::iterator preferred = itr;

    for (; itr != m_tracker_list->end(); itr++) {
      if ((*itr)->is_busy() || !(*itr)->is_usable())
        continue;

      if ((*itr)->failed_counter() <= (*preferred)->failed_counter() &&
          (*itr)->failed_time_last() < (*preferred)->failed_time_last())
        preferred = itr;
    }

    if (preferred != m_tracker_list->end())
      m_tracker_list->send_state_tracker(preferred, send_state);
  }

  if (m_slot_timeout)
    m_slot_timeout();

  // There should be no timeout when we don't have any active trackers, etc.
  // if (!m_tracker_list->has_active())
  //   update_timeout(30);
}

void
TrackerController::receive_success(Tracker* tb, TrackerController::address_list* l) {
  if (!(m_flags & flag_active)) {
    m_slot_success(l);
    return;
  }

  m_failed_requests = 0;

  // if (<check if we have multiple trackers to send this event to, before we declare success>) {
  m_flags &= ~(mask_send | flag_promiscuous_mode);
  // }

  //unsigned int next_request = m_tracker_list->focus_normal_interval();

  // If we still have active trackers, skip the timeout.

  // Calculate the next timeout according to a list of in-use
  // trackers, with the first timeout as the interval.

  if (!m_tracker_list->has_active()) {
    // For the moment, just use the last tracker...
    unsigned int next_request = tb->normal_interval();

    // Also make this check how many new peers we got, and how often
    // we've reconnected this time.
    if (m_flags & flag_requesting)
      next_request = 30;

    update_timeout(next_request);
  }

  m_slot_success(l);
}

void
TrackerController::receive_failure(Tracker* tb, const std::string& msg) {
  if (!(m_flags & flag_active)) {
    m_slot_failure(msg);
    return;
  }

  if (tb == NULL) {
    LT_LOG_TRACKER(INFO, "Received failure msg:'%s'.", msg.c_str());
    m_slot_failure(msg);
    return;
  }

  m_flags |= flag_failure_mode;

  if ((m_flags & flag_promiscuous_mode)) {
    int send_state = current_send_state();
    TrackerList::iterator itr = m_tracker_list->find_usable(m_tracker_list->begin());

    for (; itr != m_tracker_list->end(); itr++) {
      // The first time a tracker failes during promiscious requests
      // we send to all trackers.
      if ((*itr)->is_busy() || !(*itr)->is_usable() || (*itr)->failed_counter() != 0)
        continue;

      m_tracker_list->send_state_tracker(itr, send_state);
    }

    if (m_tracker_list->has_active()) {
      priority_queue_erase(&taskScheduler, &m_private->task_timeout);
      m_slot_failure(msg);
      return;
    }
  }

  // For the moment, just use the last tracker...
  unsigned int next_request = 5 << std::min<int>(tb->failed_counter() - 1, 6);

  update_timeout(next_request);
  m_slot_failure(msg);
}

void
TrackerController::receive_tracker_enabled(Tracker* tb) {
  // TODO: This won't be needed if we rely only on Tracker::m_enable,
  // rather than a virtual function.
  if (!m_tracker_list->has_usable())
    return;
  
  if ((m_flags & flag_active)) {
    if (!m_private->task_timeout.is_queued() && !m_tracker_list->has_active()) {
      // TODO: Figure out the proper timeout to use here based on when the
      // tracker last connected, etc.
      update_timeout(0);
    }
  }

  if (m_slot_tracker_enabled)
    m_slot_tracker_enabled(tb);
}

void
TrackerController::receive_tracker_disabled(Tracker* tb) {
  if (m_slot_tracker_disabled)
    m_slot_tracker_disabled(tb);
}

}

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

#include "exceptions.h"
#include "download_info.h"
#include "tracker.h"
#include "tracker_controller.h"
#include "tracker_list.h"

#include "rak/priority_queue_default.h"
#include "utils/log.h"

#include "globals.h"

#define LT_LOG_TRACKER(log_level, log_fmt, ...)                         \
  lt_log_print_info(LOG_TRACKER_##log_level, m_tracker_list->info(), "tracker_controller", log_fmt, __VA_ARGS__);

namespace torrent {

struct tracker_controller_private {
  rak::priority_item task_timeout;
  rak::priority_item task_scrape;
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
  case flag_send_update:
  default:                  return Tracker::EVENT_NONE;
  }
}

TrackerController::TrackerController(TrackerList* trackers) :
  m_flags(0),
  m_tracker_list(trackers),
  m_private(new tracker_controller_private) {

  m_private->task_timeout.slot() = std::bind(&TrackerController::do_timeout, this);
  m_private->task_scrape.slot() = std::bind(&TrackerController::do_scrape, this);
}

TrackerController::~TrackerController() {
  priority_queue_erase(&taskScheduler, &m_private->task_timeout);
  priority_queue_erase(&taskScheduler, &m_private->task_scrape);
  delete m_private;
}

rak::priority_item*
TrackerController::task_timeout() {
  return &m_private->task_timeout;
}

rak::priority_item*
TrackerController::task_scrape() {
  return &m_private->task_scrape;
}

int64_t
TrackerController::next_timeout() const {
  return m_private->task_timeout.time().usec();
}

int64_t
TrackerController::next_scrape() const {
  return m_private->task_scrape.time().usec();
}

uint32_t
TrackerController::seconds_to_next_timeout() const {
  return std::max(m_private->task_timeout.time() - cachedTime, rak::timer()).seconds_ceiling();
}

uint32_t
TrackerController::seconds_to_next_scrape() const {
  return std::max(m_private->task_scrape.time() - cachedTime, rak::timer()).seconds_ceiling();
}

void
TrackerController::manual_request(bool request_now) {
  if (!m_private->task_timeout.is_queued())
    return;

  // Add functions to get the lowest timeout, etc...
  send_update_event();
}

void
TrackerController::scrape_request(uint32_t seconds_to_request) {
  rak::timer next_timeout = cachedTime;

  if (seconds_to_request != 0)
    next_timeout = (cachedTime + rak::timer::from_seconds(seconds_to_request)).round_seconds();

  priority_queue_erase(&taskScheduler, &m_private->task_scrape);
  priority_queue_insert(&taskScheduler, &m_private->task_scrape, next_timeout);
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
  LT_LOG_TRACKER(INFO, "Sending started event.", 0);

  close();
  m_tracker_list->send_state_itr(m_tracker_list->find_usable(m_tracker_list->begin()), Tracker::EVENT_STARTED);

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

  for (TrackerList::iterator itr = m_tracker_list->begin(); itr != m_tracker_list->end(); itr++) {
    if (!(*itr)->is_in_use())
      continue;

    m_tracker_list->send_state(*itr, Tracker::EVENT_STOPPED);
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

  for (TrackerList::iterator itr = m_tracker_list->begin(); itr != m_tracker_list->end(); itr++) {
    if (!(*itr)->is_in_use())
      continue;

    m_tracker_list->send_state(*itr, Tracker::EVENT_COMPLETED);
  }

  // Timer...
}

void
TrackerController::send_update_event() {
  if (!(m_flags & flag_active) || !m_tracker_list->has_usable())
    return;

  if ((m_flags & mask_send) && m_tracker_list->has_active())
    return;

  // We can lose a state here...
  if (!(m_flags & mask_send))
    m_flags |= flag_send_update;

  LT_LOG_TRACKER(INFO, "Sending update event.", 0);

  m_tracker_list->send_state_itr(m_tracker_list->find_usable(m_tracker_list->begin()), Tracker::EVENT_NONE);

  // if (m_tracker_list->has_active())
  //   priority_queue_erase(&taskScheduler, &m_private->task_timeout);
}

// Currently being used by send_state, fixme.
void
TrackerController::close(int flags) {
  m_flags &= ~(flag_requesting | flag_promiscuous_mode);

  if ((flags & (close_disown_stop | close_disown_completed)))
    m_tracker_list->disown_all_including(close_disown_stop | close_disown_completed);

  m_tracker_list->close_all();
  priority_queue_erase(&taskScheduler, &m_private->task_timeout);
}

void
TrackerController::enable(int enable_flags) {
  if ((m_flags & flag_active))
    return;

  // Clearing send stop here in case we cycle disable/enable too
  // fast. In the future do this based on flags passed.
  m_flags |= flag_active;
  m_flags &= ~flag_send_stop;

  m_tracker_list->close_all_excluding((1 << Tracker::EVENT_COMPLETED));
  
  if (!(enable_flags & enable_dont_reset_stats))
    m_tracker_list->clear_stats();

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
  m_flags &= ~(flag_active | flag_requesting | flag_promiscuous_mode);

  m_tracker_list->close_all_excluding((1 << Tracker::EVENT_STOPPED) | (1 << Tracker::EVENT_COMPLETED));
  priority_queue_erase(&taskScheduler, &m_private->task_timeout);

  LT_LOG_TRACKER(INFO, "Called disable with %u trackers.", m_tracker_list->size());
}

void
TrackerController::start_requesting() {
  if ((m_flags & flag_requesting))
    return;

  m_flags |= flag_requesting;

  if ((m_flags & flag_active))
    update_timeout(0);

  LT_LOG_TRACKER(INFO, "Start requesting.", 0);
}

void
TrackerController::stop_requesting() {
  if (!(m_flags & flag_requesting))
    return;

  m_flags &= ~flag_requesting;

  LT_LOG_TRACKER(INFO, "Stop requesting.", 0);
}

uint32_t
tracker_next_timeout(Tracker* tracker, int controller_flags) {
  if ((controller_flags & TrackerController::flag_requesting))
    return tracker_next_timeout_promiscuous(tracker);

  if ((tracker->is_busy() && tracker->latest_event() != Tracker::EVENT_SCRAPE) ||
      !tracker->is_usable())
    return ~uint32_t();

  if ((controller_flags & TrackerController::flag_promiscuous_mode))
    return 0;
  
  if ((controller_flags & TrackerController::flag_send_update))
    return tracker_next_timeout_update(tracker);

  // if (tracker->success_counter() == 0 && tracker->failed_counter() == 0)
  //   return 0;

  int32_t last_activity = cachedTime.seconds() - tracker->activity_time_last();

  // TODO: Use min interval if we're requesting manual update.

  return tracker->normal_interval() - std::min(last_activity, (int32_t)tracker->normal_interval());
}

uint32_t
tracker_next_timeout_update(Tracker* tracker) {
  if ((tracker->is_busy() && tracker->latest_event() != Tracker::EVENT_SCRAPE) ||
      !tracker->is_usable())
    return ~uint32_t();

  // Make sure we don't request _too_ often, check last activity.
  // int32_t last_activity = cachedTime.seconds() - tracker->activity_time_last();

  return 0;
}

uint32_t
tracker_next_timeout_promiscuous(Tracker* tracker) {
  if ((tracker->is_busy() && tracker->latest_event() != Tracker::EVENT_SCRAPE) ||
      !tracker->is_usable())
    return ~uint32_t();

  int32_t interval;

  if (tracker->failed_counter())
    interval = 5 << std::min<int>(tracker->failed_counter() - 1, 6);
  else
    interval = tracker->normal_interval();

  int32_t min_interval = std::max(tracker->min_interval(), (uint32_t)300);
  int32_t use_interval = std::min(interval, min_interval);

  int32_t since_last = cachedTime.seconds() - (int32_t)tracker->activity_time_last();

  return std::max(use_interval - since_last, 0);
}

TrackerList::iterator
tracker_find_preferred(TrackerList::iterator first, TrackerList::iterator last, uint32_t* next_timeout) {
  TrackerList::iterator preferred = last;
  uint32_t preferred_time_last = ~uint32_t();

  for (; first != last; first++) {
    uint32_t tracker_timeout = tracker_next_timeout_promiscuous(*first);

    if (tracker_timeout != 0) {
      *next_timeout = std::min(tracker_timeout, *next_timeout);
      continue;
    }

    if ((*first)->activity_time_last() < preferred_time_last) {
      preferred = first;
      preferred_time_last = (*first)->activity_time_last();
    }
  }

  return preferred;
}

void
TrackerController::do_timeout() {
  if (!(m_flags & flag_active) || !m_tracker_list->has_usable())
    return;

  priority_queue_erase(&taskScheduler, &m_private->task_timeout);

  int send_state = current_send_state();

  if ((m_flags & (flag_promiscuous_mode | flag_requesting))) {
    uint32_t next_timeout = ~uint32_t();

    TrackerList::iterator itr = m_tracker_list->begin();

    while (itr != m_tracker_list->end()) {
      uint32_t group = (*itr)->group();

      if (m_tracker_list->has_active_not_scrape_in_group(group)) {
        itr = m_tracker_list->end_group(group);
        continue;
      }

      TrackerList::iterator group_end = m_tracker_list->end_group((*itr)->group());
      TrackerList::iterator preferred = itr;

      if (!(*itr)->is_usable() || (*itr)->failed_counter()) {
        // The selected tracker in the group is either disabled or not
        // reachable, try the others to find a new one to use.
        preferred = tracker_find_preferred(preferred, group_end, &next_timeout);

      } else {
        uint32_t tracker_timeout = tracker_next_timeout_promiscuous(*preferred);

        if (tracker_timeout != 0) {
          next_timeout = std::min(tracker_timeout, next_timeout);
          preferred = group_end;
        }
      }

      if (preferred != group_end)
        m_tracker_list->send_state_itr(preferred, send_state);

      itr = group_end;
    }

    if (next_timeout != ~uint32_t())
      update_timeout(next_timeout);

  // TODO: Send for start/completed also?
  } else {
    TrackerList::iterator itr = m_tracker_list->find_next_to_request(m_tracker_list->begin());

    if (itr == m_tracker_list->end())
      return;

    int32_t next_timeout = (*itr)->activity_time_next();

    if (next_timeout <= cachedTime.seconds())
      m_tracker_list->send_state_itr(itr, send_state);
    else
      update_timeout(next_timeout - cachedTime.seconds());
  }

  if (m_slot_timeout)
    m_slot_timeout();
}

void
TrackerController::do_scrape() {
  TrackerList::iterator itr = m_tracker_list->begin();

  while (itr != m_tracker_list->end()) {
    uint32_t group = (*itr)->group();

    if (m_tracker_list->has_active_in_group(group)) {
      itr = m_tracker_list->end_group(group);
      continue;
    }

    TrackerList::iterator group_end = m_tracker_list->end_group((*itr)->group());

    while (itr != group_end) {
      if ((*itr)->can_scrape() && (*itr)->is_usable()) {
        m_tracker_list->send_scrape(*itr);
        break;
      }

      itr++;
    }

    itr = group_end;
  }  
}

uint32_t
TrackerController::receive_success(Tracker* tb, TrackerController::address_list* l) {
  if (!(m_flags & flag_active))
    return m_slot_success(l);

  // if (<check if we have multiple trackers to send this event to, before we declare success>) {
  m_flags &= ~(mask_send | flag_promiscuous_mode | flag_failure_mode);
  // }

  // If we still have active trackers, skip the timeout.

  // Calculate the next timeout according to a list of in-use
  // trackers, with the first timeout as the interval.

  if ((m_flags & flag_requesting))
    update_timeout(30);
  else if (!m_tracker_list->has_active())
    // TODO: Instead find the lowest timeout, correct timeout?
    update_timeout(tb->normal_interval());

  return m_slot_success(l);
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

  if (tb->failed_counter() == 1 && tb->success_counter() > 0)
    m_flags |= flag_failure_mode;

  do_timeout();
  m_slot_failure(msg);
}

void
TrackerController::receive_scrape(Tracker* tb) {
  if (!(m_flags & flag_active)) {
    return;
  }
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
  if ((m_flags & flag_active) && !m_private->task_timeout.is_queued())
    update_timeout(0);

  if (m_slot_tracker_disabled)
    m_slot_tracker_disabled(tb);
}

}

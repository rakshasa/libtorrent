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

#include "utils/log.h"
#include "tracker/tracker_dht.h"
#include "tracker/tracker_http.h"
#include "tracker/tracker_manager.h"
#include "tracker/tracker_udp.h"

#include "globals.h"

#define LT_LOG_TRACKER(log_level, log_fmt, ...)                         \
  lt_log_print_info(LOG_TRACKER_##log_level, m_tracker_list->info(), "->tracker_controller: " log_fmt, __VA_ARGS__);

namespace torrent {

TrackerController::TrackerController(TrackerList* trackers) :
  m_flags(0),
  m_tracker_list(trackers),
  m_failed_requests(0),
  m_task_timeout(new rak::priority_item()) {
}

TrackerController::~TrackerController() {
  delete m_task_timeout;
}

int64_t
TrackerController::next_timeout() const {
  return m_task_timeout->time().usec();
}

uint32_t
TrackerController::seconds_to_next_timeout() const {
  return std::max(m_task_timeout->time() - cachedTime, rak::timer()).seconds();
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

void
TrackerController::send_start_event() {

  // This will now be 'lazy', rather than a definite event. We tell
  // the controller that a 'start' event should be sent, and it will
  // send it when the tracker controller get's enabled.

  // If the controller is already running, we insert this new event.
  
  // Return, or something, if already active and sending?

  // m_flags &= ~(flag_send_stop | flag_send_completed);
  // m_flags |= flag_send_start;
  
  // if (m_flags & flag_active) {
    // Start with requesting from the first

    // Print log. Add macro for prefix.
  // } else {
    // Print log.
  // }
}

// Currently being used by send_state, fixme.
void
TrackerController::close() {
  m_failed_requests = 0;

  stop_requesting();

  m_tracker_list->close_all();
  priority_queue_erase(&taskScheduler, m_task_timeout);
}

void
TrackerController::enable() {
  m_flags |= flag_active;

  LT_LOG_TRACKER(INFO, "Called enable with %u trackers.", m_tracker_list->size());

  // Start sending...
}

void
TrackerController::disable() {
  m_flags &= ~flag_active;

  LT_LOG_TRACKER(INFO, "Called disable with %u trackers.", m_tracker_list->size());
}

void
TrackerController::start_requesting() {
  m_flags |= flag_requesting;
}

void
TrackerController::stop_requesting() {
  m_flags &= ~flag_requesting;
}

void
TrackerController::receive_success(Tracker* tb, TrackerController::address_list* l) {
  m_failed_requests = 0;

  LT_LOG_TRACKER(INFO, "Received %u peers from tracker url:'%s'.", l->size(), tb->url().c_str());

  m_slot_success(l);
}

void
TrackerController::receive_failure(Tracker* tb, const std::string& msg) {
  if (tb == NULL) {
    LT_LOG_TRACKER(INFO, "Received failure msg:'%s'.", msg.c_str());
    m_slot_failure(msg);
    return;
  }

  LT_LOG_TRACKER(INFO, "Received failure to connect to tracker url:'%s' msg:'%s'.", tb->url().c_str(), msg.c_str());

  m_slot_failure(msg);
}

}

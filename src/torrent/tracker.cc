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

#include <algorithm>

#include "exceptions.h"
#include "globals.h"
#include "tracker.h"
#include "tracker_list.h"

namespace torrent {

Tracker::Tracker(TrackerList* parent, const std::string& url, int flags) :
  m_flags(flags),
  m_parent(parent),
  m_group(0),
  m_url(url),

  m_normal_interval(1800),
  m_min_interval(600),

  m_latest_event(EVENT_NONE),
  m_latest_new_peers(0),
  m_latest_sum_peers(0),

  m_success_time_last(0),
  m_success_counter(0),

  m_failed_time_last(0),
  m_failed_counter(0),

  m_scrape_time_last(0),
  m_scrape_counter(0),

  m_scrape_complete(0),
  m_scrape_incomplete(0),
  m_scrape_downloaded(0),

  m_request_time_last(torrent::cachedTime.seconds()),
  m_request_counter(0)
{
}

void
Tracker::enable() {
  if (is_enabled())
    return;

  m_flags |= flag_enabled;
  
  if (m_parent->slot_tracker_enabled())
    m_parent->slot_tracker_enabled()(this);
}

void
Tracker::disable() {
  if (!is_enabled())
    return;

  close();
  m_flags &= ~flag_enabled;

  if (m_parent->slot_tracker_disabled())
    m_parent->slot_tracker_disabled()(this);
}

uint32_t
Tracker::success_time_next() const {
  if (m_success_counter == 0)
    return 0;

  return m_success_time_last + m_normal_interval;
}

uint32_t
Tracker::failed_time_next() const {
  if (m_failed_counter == 0)
    return 0;

  return m_failed_time_last + (5 << std::min(m_failed_counter - 1, (uint32_t)6));
}

std::string
Tracker::scrape_url_from(std::string url) {
  size_t delim_slash = url.rfind('/');

  if (delim_slash == std::string::npos || url.find("/announce", delim_slash) != delim_slash)
    throw internal_error("Tried to make scrape url from invalid url.");

  return url.replace(delim_slash, sizeof("/announce") - 1, "/scrape");
}

void
Tracker::send_scrape() {
  throw internal_error("Tracker type does not support scrape.");
}

void
Tracker::inc_request_counter() {
  m_request_counter -= std::min(m_request_counter, (uint32_t)cachedTime.seconds() - m_request_time_last);
  m_request_counter++;
  m_request_time_last = cachedTime.seconds();

  if (m_request_counter >= 10)
    throw internal_error("Tracker request had more than 10 requests in 10 seconds.");
}

void
Tracker::clear_stats() {
  m_latest_new_peers = 0;
  m_latest_sum_peers = 0;

  m_success_counter = 0;
  m_failed_counter = 0;
  m_scrape_counter = 0;
}

}

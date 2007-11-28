// libTorrent - BitTorrent library
// Copyright (C) 2005-2007, Jari Sundell
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

#include <sstream>

#include "dht/dht_router.h"
#include "torrent/connection_manager.h"
#include "torrent/dht_manager.h"
#include "torrent/exceptions.h"
#include "torrent/tracker_list.h"

#include "tracker_dht.h"

#include "globals.h"
#include "manager.h"

namespace torrent {

const char* TrackerDht::states[] = { "Idle", "Searching", "Announcing" };

TrackerDht::TrackerDht(TrackerList* parent, const std::string& url) :
  Tracker(parent, url),
  m_state(state_idle) {

  if (!manager->dht_manager()->is_valid())
    throw internal_error("Trying to add DHT tracker with no DHT manager.");
}

TrackerDht::~TrackerDht() {
  if (is_busy())
    manager->dht_manager()->router()->cancel_announce(NULL, this);
}

bool
TrackerDht::is_busy() const {
  return m_state != state_idle;
}

bool
TrackerDht::is_usable() const {
  return m_enabled && manager->dht_manager()->is_active();
}

void
TrackerDht::send_state(int state) {
  if (m_parent == NULL)
    throw internal_error("TrackerDht::send_state(...) does not have a valid m_parent.");

  if (is_busy()) {
    manager->dht_manager()->router()->cancel_announce(m_parent->info(), this);

    if (is_busy())
      throw internal_error("TrackerDht::send_state cancel_announce did not cancel announce.");
  }

  if (state == DownloadInfo::STOPPED)
    return;

  m_state = state_searching;

  if (!manager->dht_manager()->is_active())
    return receive_failed("DHT server not active.");

  manager->dht_manager()->router()->announce(m_parent->info(), this);

  set_normal_interval(20 * 60);
  set_min_interval(0);
}

void
TrackerDht::close() {
  if (is_busy())
    manager->dht_manager()->router()->cancel_announce(m_parent->info(), this);
}

TrackerDht::Type
TrackerDht::type() const {
  return TRACKER_DHT;
}

void
TrackerDht::receive_peers(const std::string& peers) {
  if (!is_busy())
    throw internal_error("TrackerDht::receive_peers called while not busy.");

  m_peers.parse_address_compact(peers);
}

void
TrackerDht::receive_success() {
  if (!is_busy())
    throw internal_error("TrackerDht::receive_success called while not busy.");

  m_state = state_idle;
  m_parent->receive_success(this, &m_peers);
  m_peers.clear();
}

void
TrackerDht::receive_failed(const char* msg) {
  if (!is_busy())
    throw internal_error("TrackerDht::receive_failed called while not busy.");

  m_state = state_idle;
  m_parent->receive_failed(this, msg);
  m_peers.clear();
}

void
TrackerDht::receive_progress(int replied, int contacted) {
  if (!is_busy())
    throw internal_error("TrackerDht::receive_status called while not busy.");

  m_replied = replied;
  m_contacted = contacted;
}

void
TrackerDht::get_status(char* buffer, int length) {
  if (!is_busy())
    throw internal_error("TrackerDht::get_status called while not busy.");
  
  snprintf(buffer, length, "[%s: %d/%d nodes replied]", states[m_state], m_replied, m_contacted);
}

}

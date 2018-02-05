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

#include <sstream>
#include <cstdio>

#include "dht/dht_router.h"
#include "torrent/connection_manager.h"
#include "torrent/download_info.h"
#include "torrent/dht_manager.h"
#include "torrent/exceptions.h"
#include "torrent/tracker_list.h"
#include "torrent/utils/log.h"

#include "tracker_webseed.h"

#include "globals.h"
#include "manager.h"

namespace torrent {

const char* TrackerWebseed::states[] = { "Idle", "Downloading" };

TrackerWebseed::TrackerWebseed(TrackerList* parent, const std::string& url, int flags) :
  Tracker(parent, url, flags),
  m_state(state_idle) {
}

TrackerWebseed::~TrackerWebseed() {
}

bool
TrackerWebseed::is_busy() const {
  return m_state != state_idle;
}

bool
TrackerWebseed::is_usable() const {
  return true; 
}

void
TrackerWebseed::send_state(int state) {
  if (m_parent == NULL)
    throw internal_error("TrackerWebseed::send_state(...) does not have a valid m_parent.");

  m_latest_event = state;

  if (state == DownloadInfo::STOPPED)
    return;

}

void
TrackerWebseed::close() {
}

void
TrackerWebseed::disown() {
  close();
}

TrackerWebseed::Type
TrackerWebseed::type() const {
  return TRACKER_WEBSEED;
}

void
TrackerWebseed::get_status(char* buffer, int length) {
  if (!is_busy())
    throw internal_error("TrackerWebseed::get_status called while not busy.");
}

}

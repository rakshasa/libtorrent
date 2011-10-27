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

#include "torrent/download_info.h"
#include "torrent/exceptions.h"
#include "torrent/tracker.h"
#include "torrent/tracker_list.h"

#include "tracker_dht.h"
#include "tracker_http.h"
#include "tracker_manager.h"
#include "tracker_udp.h"

namespace std { using namespace tr1; }

namespace torrent {

TrackerManager::TrackerManager() :
  m_tracker_list(new TrackerList()) {

  m_tracker_controller = new TrackerController(m_tracker_list);

  m_tracker_list->slot_success() = std::bind(&TrackerController::receive_success_new, m_tracker_controller, std::placeholders::_1, std::placeholders::_2);
  m_tracker_list->slot_failure() = std::bind(&TrackerController::receive_failure_new, m_tracker_controller, std::placeholders::_1, std::placeholders::_2);
}

TrackerManager::~TrackerManager() {
  if (m_tracker_controller->is_active())
    throw internal_error("TrackerManager::~TrackerManager() called but is_active() != false.");

  m_tracker_list->clear();
  delete m_tracker_list;
  delete m_tracker_controller;
}

}

// libTorrent - BitTorrent library
// Copyright (C) 2005, Jari Sundell
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

#include <sigc++/signal.h>

#include "tracker/tracker_info.h"
#include "tracker/tracker_manager.h"

#include "download_main.h"
#include "choke_manager.h"

namespace torrent {

void
DownloadMain::setup_delegator() {
  m_delegator.get_select().set_bitfield(&m_content.bitfield());
  m_delegator.get_select().set_seen(&m_bitfieldCounter);

  m_delegator.slot_chunk_done(rak::make_mem_fn(this, &DownloadMain::receive_chunk_done));
  m_delegator.slot_chunk_size(rak::make_mem_fn(&m_content, &Content::chunk_index_size));
}

void
DownloadMain::setup_tracker() {
  // This must be done before adding to available addresses.
//   m_trackerManager->signal_success().connect(sigc::mem_fun(*connection_list(), &ConnectionList::set_difference));
//   m_trackerManager->signal_success().connect(sigc::mem_fun(*available_list(), &AvailableList::insert));
//   m_trackerManager->signal_success().connect(sigc::hide(sigc::mem_fun(*this, &DownloadMain::receive_connect_peers)));
//   m_trackerManager->signal_success().connect(sigc::hide(sigc::mem_fun(*this, &DownloadMain::receive_tracker_success)));

  m_trackerManager->tracker_info()->slot_stat_down() = rak::make_mem_fn(&m_downRate, &Rate::total);
  m_trackerManager->tracker_info()->slot_stat_up()   = rak::make_mem_fn(&m_upRate, &Rate::total);
  m_trackerManager->tracker_info()->slot_stat_left() = rak::make_mem_fn(this, &DownloadMain::get_bytes_left);
}

void
DownloadMain::setup_start() {
//   m_connectionChunkPassed = signal_chunk_passed().connect(sigc::mem_fun(m_delegator, &Delegator::done));
//   m_connectionChunkFailed = signal_chunk_failed().connect(sigc::mem_fun(m_delegator, &Delegator::redo));

  taskScheduler.insert(&m_taskTick, (Timer::cache() + 2 * 30 * 1000000).round_seconds());
  m_content.block_download_done(false);
}

void
DownloadMain::setup_stop() {
//   m_connectionChunkPassed.disconnect();
//   m_connectionChunkFailed.disconnect();

  taskScheduler.erase(&m_taskTick);
  taskScheduler.erase(&m_taskTrackerRequest);
  m_content.block_download_done(true);
}

}

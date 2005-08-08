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
#include <sigc++/hide.h>
#include <sigc++/bind.h>

#include "torrent/exceptions.h"
#include "tracker/tracker_control.h"
#include "utils/string_manip.h"

#include "download_main.h"

namespace torrent {

void
DownloadMain::setup_delegator() {
  m_net.get_delegator().get_select().set_bitfield(&m_state.get_content().get_bitfield());
  m_net.get_delegator().get_select().set_seen(&m_state.get_bitfield_counter());

  m_net.get_delegator().signal_chunk_done().connect(sigc::mem_fun(m_state, &DownloadState::chunk_done));
  m_net.get_delegator().slot_chunk_size(sigc::mem_fun(m_state.get_content(), &Content::get_chunksize));
}

void
DownloadMain::setup_net() {
  m_net.set_settings(&m_settings);
  m_state.set_settings(&m_settings);

  // TODO: Consider disabling these during hash check.
  m_state.signal_chunk_passed().connect(sigc::mem_fun(m_net, &DownloadNet::send_have_chunk));

  // This is really _state stuff:
  m_state.slot_set_endgame(sigc::mem_fun(m_net, &DownloadNet::set_endgame));
  m_state.slot_delegated_chunks(sigc::mem_fun(m_net.get_delegator().get_chunks(), &Delegator::Chunks::size));

  m_net.get_connection_list().signal_peer_connected().connect(sigc::mem_fun(m_net, &DownloadNet::receive_remove_available));
  m_net.get_connection_list().signal_peer_disconnected().connect(sigc::hide(sigc::mem_fun(m_net, &DownloadNet::choke_balance)));
  m_net.get_connection_list().signal_peer_disconnected().connect(sigc::hide(sigc::mem_fun(m_net, &DownloadNet::connect_peers)));
}

void
DownloadMain::setup_tracker() {
  // This must be done before adding to available addresses.
  m_tracker.tracker_control()->signal_success().connect(sigc::mem_fun(m_net.get_connection_list(), &ConnectionList::remove_connected));
  m_tracker.tracker_control()->signal_success().connect(sigc::mem_fun(m_net.get_available_list(), &AvailableList::insert));
  m_tracker.tracker_control()->signal_success().connect(sigc::hide(sigc::mem_fun(m_net, &DownloadNet::connect_peers)));

  m_tracker.tracker_control()->slot_stat_down(sigc::mem_fun(m_net.get_read_rate(), &Rate::total));
  m_tracker.tracker_control()->slot_stat_up(sigc::mem_fun(m_net.get_write_rate(), &Rate::total));
  m_tracker.tracker_control()->slot_stat_left(sigc::mem_fun(m_state, &DownloadState::bytes_left));
}

void
DownloadMain::setup_start() {
  m_connectionChunkPassed = m_state.signal_chunk_passed().connect(sigc::mem_fun(m_net.get_delegator(), &Delegator::done));
  m_connectionChunkFailed = m_state.signal_chunk_failed().connect(sigc::mem_fun(m_net.get_delegator(), &Delegator::redo));

  taskScheduler.insert(&m_taskChokeCycle, Timer::cache() + m_state.get_settings().chokeCycle * 2);
  m_state.get_content().block_download_done(false);
}

void
DownloadMain::setup_stop() {
  m_connectionChunkPassed.disconnect();
  m_connectionChunkFailed.disconnect();

  taskScheduler.erase(&m_taskChokeCycle);
  m_state.get_content().block_download_done(true);
}

}

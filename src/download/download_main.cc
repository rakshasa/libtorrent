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

#include <limits>
#include <sigc++/signal.h>

#include "content/delegator_select.h"
#include "parse/parse.h"
#include "protocol/handshake_manager.h"
#include "torrent/exceptions.h"

#include "download_main.h"
#include "peer_connection.h"

namespace torrent {

DownloadMain::DownloadMain() :
  m_checked(false),
  m_started(false),
  m_endgame(false),

  m_writeRate(60),
  m_readRate(60) {

  m_state.get_content().block_download_done(true);

  m_taskChokeCycle.set_slot(sigc::mem_fun(*this, &DownloadMain::receive_choke_cycle));
  m_taskChokeCycle.set_iterator(taskScheduler.end());

  m_taskTrackerRequest.set_slot(sigc::mem_fun(*this, &DownloadMain::receive_tracker_request));
  m_taskTrackerRequest.set_iterator(taskScheduler.end());
}

DownloadMain::~DownloadMain() {
  if (taskScheduler.is_scheduled(&m_taskChokeCycle) ||
      taskScheduler.is_scheduled(&m_taskTrackerRequest))
    throw internal_error("DownloadMain::~DownloadMain(): m_taskChokeCycle is scheduled");
}

void
DownloadMain::open() {
  if (is_open())
    throw internal_error("Tried to open a download that is already open");

  m_state.get_content().open();
  m_state.get_bitfield_counter().create(m_state.get_chunk_total());

  m_delegator.get_select().get_priority().add(Priority::NORMAL, 0, m_state.get_chunk_total());
}

void
DownloadMain::close() {
  if (is_active())
    throw internal_error("Tried to close an active download");

  m_checked = false;

  m_tracker.close();
  m_state.get_content().close();
  m_delegator.clear();
}

void DownloadMain::start() {
  if (!m_state.get_content().is_open())
    throw client_error("Tried to start a closed download");

  if (!is_checked())
    throw client_error("Tried to start an unchecked download");

  if (is_active())
    return;

  m_started = true;
  m_lastConnectedSize = 0;

  setup_start();
  m_tracker.send_start();

  receive_connect_peers();
}  

void
DownloadMain::stop() {
  if (!m_started)
    return;

  // Set this early so functions like receive_connect_peers() knows
  // not to eat available peers.
  m_started = false;

  // Save the addresses we are connected to, so we don't need to
  // perform alot of requests to the tracker when restarting the
  // torrent. Consider saving these in the torrent file when dumping
  // it.
  std::list<SocketAddress> addressList;

  std::transform(connection_list()->begin(), connection_list()->end(), std::back_inserter(addressList),
		 rak::on(std::mem_fun(&PeerConnection::get_peer), std::mem_fun_ref(&PeerInfo::get_socket_address)));

  addressList.sort();
  available_list()->insert(&addressList);

  while (!connection_list()->empty())
    connection_list()->erase(connection_list()->front());

  m_tracker.send_stop();
  setup_stop();
}

void
DownloadMain::set_endgame(bool b) {
  m_endgame = b;
  m_delegator.set_aggressive(b);
}

void
DownloadMain::receive_choke_cycle() {
  taskScheduler.insert(&m_taskChokeCycle, (Timer::cache() + 30 * 1000000).round_seconds());
  choke_cycle();
}

void
DownloadMain::receive_connect_peers() {
  if (!m_started)
    return;

  while (!available_list()->empty() &&
	 connection_list()->size() < connection_list()->get_min_size() &&
	 connection_list()->size() + m_slotCountHandshakes() < connection_list()->get_max_size()) {
    SocketAddress sa = available_list()->pop_random();

    if (connection_list()->find(sa) == connection_list()->end())
      m_slotStartHandshake(sa);
  }
}

void
DownloadMain::receive_initial_hash() {
  if (m_checked)
    throw internal_error("DownloadMain::receive_initial_hash() called but m_checked == true");

  m_checked = true;
  m_state.get_content().resize();
}    

void
DownloadMain::receive_tracker_success() {
  if (!m_started)
    return;

  taskScheduler.erase(&m_taskTrackerRequest);
  taskScheduler.insert(&m_taskTrackerRequest, (Timer::cache() + 30 * 1000000).round_seconds());
}

void
DownloadMain::receive_tracker_request() {
  if (connection_list()->size() >= connection_list()->get_min_size())
    return;

  if (connection_list()->size() >= m_lastConnectedSize + 10)
    m_tracker.request_current();
  else // Check to make sure we don't query after every connection to the primary tracker?
    m_tracker.request_next();

  m_lastConnectedSize = connection_list()->size();
}

}

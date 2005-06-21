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
// Contact:  Jari Sundell <jaris@ifi.uio.no>
//
//           Skomakerveien 33
//           3185 Skoppum, NORWAY

#include "config.h"

#include <sigc++/signal.h>

#include "torrent/exceptions.h"
#include "net/handshake_manager.h"
#include "parse/parse.h"
#include "content/delegator_select.h"

#include "download_main.h"
#include "peer_connection.h"

#include <limits>

namespace torrent {

DownloadMain::DownloadMain() :
  m_settings(DownloadSettings::global()),
  m_tracker(NULL),
  m_checked(false),
  m_started(false),
  m_taskChokeCycle(sigc::mem_fun(*this, &DownloadMain::choke_cycle))
{
  m_state.get_content().signal_download_done().connect(sigc::mem_fun(*this, &DownloadMain::receive_download_done));
  m_state.get_content().block_download_done(true);
}

DownloadMain::~DownloadMain() {
  delete m_tracker;
}

void
DownloadMain::open() {
  if (is_open())
    throw internal_error("Tried to open a download that is already open");

  m_state.get_content().open();
  m_state.get_bitfield_counter().create(m_state.get_chunk_total());

  m_net.get_delegator().get_select().get_priority().add(Priority::NORMAL, 0, m_state.get_chunk_total());
}

void
DownloadMain::close() {
  if (is_active())
    throw internal_error("Tried to close an active download");

  m_checked = false;
  m_state.get_content().close();
  m_net.get_delegator().clear();
  m_tracker->cancel();
}

void DownloadMain::start() {
  if (!m_state.get_content().is_open())
    throw client_error("Tried to start a closed download");

  if (!is_checked())
    throw client_error("Tried to start an unchecked download");

  if (is_active())
    return;

  m_started = true;
  m_tracker->send_state(TrackerInfo::STARTED);

  setup_start();
}  

void DownloadMain::stop() {
  if (!m_started)
    return;

  while (!m_net.get_connections().empty())
    m_net.remove_connection(m_net.get_connections().front());

  m_tracker->send_state(TrackerInfo::STOPPED);
  m_started = false;

  setup_stop();
}

void
DownloadMain::choke_cycle() {
  m_taskChokeCycle.insert(Timer::cache() + m_state.get_settings().chokeCycle);
  m_net.choke_cycle();
}

void DownloadMain::receive_initial_hash() {
  if (m_checked)
    throw internal_error("DownloadMain::receive_initial_hash() called but m_checked == true");

  m_checked = true;
  m_state.get_content().resize();
}    

void
DownloadMain::receive_download_done() {
  if (!m_started || !m_checked || m_tracker->get_state() == TrackerInfo::STARTED)
    throw internal_error("DownloadMain::receive_download_done() called on a download in an invalid state");

  m_tracker->send_state(TrackerInfo::COMPLETED);
}

}

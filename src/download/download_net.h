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

#ifndef LIBTORRENT_DOWNLOAD_NET_H
#define LIBTORRENT_DOWNLOAD_NET_H

#include <deque>
#include <inttypes.h>

#include "content/delegator.h"
#include "net/socket_address.h"
#include "net/socket_fd.h"
#include "torrent/peer.h"
#include "torrent/rate.h"

#include "available_list.h"
#include "choke_manager.h"
#include "connection_list.h"

namespace torrent {

class DownloadSettings;
class PeerConnectionBase;

class DownloadNet {
public:
  DownloadNet();

  uint32_t            pipe_size(const Rate& r);

  bool                should_request(uint32_t stall);

  bool                get_endgame()                            { return m_endgame; }
  void                set_endgame(bool b);

  void                set_settings(DownloadSettings* s)        { m_settings = s; }

  Delegator&          get_delegator()                          { return m_delegator; }
  ChokeManager&       get_choke_manager()                      { return m_chokeManager; }

  AvailableList&      available_list()                         { return m_availableList; }
  ConnectionList&     connection_list()                        { return m_connectionList; }

  Rate&               get_write_rate()                         { return m_writeRate; }
  Rate&               get_read_rate()                          { return m_readRate; }

  void                send_have_chunk(uint32_t index);

  uint32_t            count_connections() const; 

  void                choke_balance();
  void                choke_cycle();
  int                 get_unchoked();

  void                receive_remove_available(Peer p);

  // Signals and slots.

  typedef sigc::signal1<void, const std::string&>                      SignalString;

  typedef sigc::slot1<bool, const SocketAddress&>                      SlotHasHandshake;
  typedef sigc::slot0<uint32_t>                                        SlotCountHandshakes;

  SignalString& signal_network_log()                                   { return m_signalNetworkLog; }

  void          slot_has_handshake(SlotHasHandshake s)                 { m_slotHasHandshake = s; }
  void          slot_count_handshakes(SlotCountHandshakes s)           { m_slotCountHandshakes = s; }

private:
  DownloadNet(const DownloadNet&);
  void operator = (const DownloadNet&);

  DownloadSettings*      m_settings;
  Delegator              m_delegator;

  AvailableList          m_availableList;
  ConnectionList         m_connectionList;

  ChokeManager           m_chokeManager;

  bool                   m_endgame;
  
  Rate                   m_writeRate;
  Rate                   m_readRate;

  SignalString           m_signalNetworkLog;

  SlotHasHandshake       m_slotHasHandshake;
  SlotCountHandshakes    m_slotCountHandshakes;
};

inline void
DownloadNet::choke_balance() {
  m_chokeManager.balance(m_connectionList.begin(), m_connectionList.end());
}

inline void
DownloadNet::choke_cycle() {
  m_chokeManager.cycle(m_connectionList.begin(), m_connectionList.end());
}

inline int
DownloadNet::get_unchoked() {
  return m_chokeManager.get_unchoked(m_connectionList.begin(), m_connectionList.end());
}

}

#endif

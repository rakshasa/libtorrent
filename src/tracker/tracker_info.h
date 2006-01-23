// libTorrent - BitTorrent library
// Copyright (C) 2005-2006, Jari Sundell
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

#ifndef LIBTORRENT_TRACKER_INFO_H
#define LIBTORRENT_TRACKER_INFO_H

#include <list>
#include <string>
#include <inttypes.h>
#include <rak/functional.h>

#include "net/socket_address.h"

namespace torrent {

class DownloadMain;
class Rate;

class TrackerInfo {
public:
  typedef std::list<SocketAddress>                   AddressList;

  typedef rak::const_mem_fun0<DownloadMain, uint64_t> SlotStat;
  typedef rak::const_mem_fun0<Rate, uint64_t>         SlotStatRate;

  enum State {
    NONE,
    COMPLETED,
    STARTED,
    STOPPED
  };

  TrackerInfo() : m_key(0), m_compact(true), m_numwant(-1) {}

  const std::string&  get_hash() const                             { return m_hash; }
  void                set_hash(const std::string& hash)            { m_hash = hash; }

  const std::string&  get_local_id() const                         { return m_localId; }
  void                set_local_id(const std::string& id)          { m_localId = id; }

  SocketAddress&      bind_address()                               { return m_bindAddress; }
  SocketAddress&      local_address()                              { return m_localAddress; }

  uint32_t            get_key() const                              { return m_key; }
  void                set_key(uint32_t key)                        { m_key = key; }

  bool                get_compact() const                          { return m_compact; }
  int32_t             get_numwant() const                          { return m_numwant; }

  void                set_compact(bool c)                          { m_compact = c; }
  void                set_numwant(int32_t n)                       { m_numwant = n; }
  
  uint32_t            http_timeout() const                         { return 60; }
  uint32_t            udp_timeout() const                          { return 30; }
  uint32_t            udp_tries() const                            { return 2; }

  SlotStatRate&       slot_stat_down()                             { return m_slotStatDown; }
  SlotStatRate&       slot_stat_up()                               { return m_slotStatUp; }
  SlotStat&           slot_stat_left()                             { return m_slotStatLeft; }

private:
  std::string         m_hash;
  std::string         m_localId;

  SocketAddress       m_bindAddress;
  SocketAddress       m_localAddress;

  uint32_t            m_key;
  bool                m_compact;
  int32_t             m_numwant;

  SlotStatRate        m_slotStatDown;
  SlotStatRate        m_slotStatUp;
  SlotStat            m_slotStatLeft;
};

}

#endif

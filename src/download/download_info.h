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
#include <rak/socket_address.h>
#include <sigc++/signal.h>

#include "torrent/rate.h"

namespace torrent {

class DownloadMain;
class Rate;

// This will become a Download 'handle' of kinds.

class DownloadInfo {
public:
  typedef std::list<rak::socket_address>              AddressList;

  typedef rak::const_mem_fun0<DownloadMain, uint64_t> SlotStat;
  typedef rak::const_mem_fun0<Rate, uint64_t>         SlotStatRate;

  enum State {
    NONE,
    COMPLETED,
    STARTED,
    STOPPED
  };

  DownloadInfo() :
    m_port(0),
    m_key(0),
    m_compact(true),
    m_numwant(-1),
    m_upRate(60),
    m_downRate(60) {
  }

  const std::string&  name() const                                 { return m_name; }
  void                set_name(const std::string& s)               { m_name = s; }

  const std::string&  hash() const                                 { return m_hash; }
  void                set_hash(const std::string& hash)            { m_hash = hash; }

  const std::string&  local_id() const                             { return m_localId; }
  void                set_local_id(const std::string& id)          { m_localId = id; }

  uint16_t            port() const                                 { return m_port; }
  void                set_port(uint16_t p)                         { m_port = p; }

  uint32_t            key() const                                  { return m_key; }
  void                set_key(uint32_t key)                        { m_key = key; }

  bool                compact() const                              { return m_compact; }
  void                set_compact(bool c)                          { m_compact = c; }

  int32_t             numwant() const                              { return m_numwant; }
  void                set_numwant(int32_t n)                       { m_numwant = n; }
  
  Rate*               up_rate()                                    { return &m_upRate; }
  Rate*               down_rate()                                  { return &m_downRate; }

  uint32_t            http_timeout() const                         { return 60; }
  uint32_t            udp_timeout() const                          { return 30; }
  uint32_t            udp_tries() const                            { return 2; }

  SlotStat&           slot_stat_left()                             { return m_slotStatLeft; }

  typedef sigc::signal1<void, const std::string&> SignalString;
  typedef sigc::signal1<void, uint32_t>           SignalChunk;

  SignalString&       signal_network_log()                         { return m_signalNetworkLog; }
  SignalString&       signal_storage_error()                       { return m_signalStorageError; }

private:
  std::string         m_name;
  std::string         m_hash;
  std::string         m_localId;

  uint16_t            m_port;

  uint32_t            m_key;
  bool                m_compact;
  int32_t             m_numwant;

  Rate                m_upRate;
  Rate                m_downRate;

  SlotStat            m_slotStatLeft;

  SignalString        m_signalNetworkLog;
  SignalString        m_signalStorageError;
};

// Move somewhere else.
struct SocketAddressCompact {
  SocketAddressCompact() {}
  SocketAddressCompact(uint32_t a, uint16_t p) : addr(a), port(p) {}

  operator rak::socket_address () const {
    rak::socket_address sa;
    sa.sa_inet()->clear();
    sa.sa_inet()->set_port_n(port);
    sa.sa_inet()->set_address_n(addr);

    return sa;
  }

  uint32_t addr;
  uint16_t port;

  const char*         c_str() const { return reinterpret_cast<const char*>(this); }
} __attribute__ ((packed));

}

#endif

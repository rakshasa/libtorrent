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
  typedef rak::const_mem_fun0<DownloadMain, uint64_t>                  slot_stat_type;

  typedef sigc::signal1<void, const std::string&>                      signal_string_type;
  typedef sigc::signal1<void, uint32_t>                                signal_chunk_type;
  typedef sigc::signal3<void, const std::string&, const char*, size_t> signal_dump_type;

  enum State {
    NONE,
    COMPLETED,
    STARTED,
    STOPPED
  };

  DownloadInfo() :
    m_key(0),
    m_compact(true),
    m_numwant(-1),

    m_upRate(60),
    m_downRate(60),
    m_uploadedBaseline(0) {
  }

  const std::string&  name() const                                 { return m_name; }
  void                set_name(const std::string& s)               { m_name = s; }

  const std::string&  hash() const                                 { return m_hash; }
  void                set_hash(const std::string& hash)            { m_hash = hash; }

  const std::string&  local_id() const                             { return m_localId; }
  void                set_local_id(const std::string& id)          { m_localId = id; }

  uint32_t            key() const                                  { return m_key; }
  void                set_key(uint32_t key)                        { m_key = key; }

  bool                compact() const                              { return m_compact; }
  void                set_compact(bool c)                          { m_compact = c; }

  int32_t             numwant() const                              { return m_numwant; }
  void                set_numwant(int32_t n)                       { m_numwant = n; }
  
  Rate*               up_rate()                                    { return &m_upRate; }
  Rate*               down_rate()                                  { return &m_downRate; }

  uint64_t            uploaded_baseline() const                    { return m_uploadedBaseline; }
  void                set_uploaded_baseline(uint64_t b)            { m_uploadedBaseline = b; }

  uint32_t            http_timeout() const                         { return 60; }
  uint32_t            udp_timeout() const                          { return 30; }
  uint32_t            udp_tries() const                            { return 2; }

  slot_stat_type&     slot_stat_left()                             { return m_slotStatLeft; }

  signal_string_type& signal_network_log()                         { return m_signalNetworkLog; }
  signal_string_type& signal_storage_error()                       { return m_signalStorageError; }
  signal_dump_type&   signal_tracker_dump()                        { return m_signalTrackerDump; }

private:
  std::string         m_name;
  std::string         m_hash;
  std::string         m_localId;

  uint32_t            m_key;
  bool                m_compact;
  int32_t             m_numwant;

  Rate                m_upRate;
  Rate                m_downRate;

  uint64_t            m_uploadedBaseline;

  slot_stat_type      m_slotStatLeft;

  signal_string_type  m_signalNetworkLog;
  signal_string_type  m_signalStorageError;
  signal_dump_type    m_signalTrackerDump;
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

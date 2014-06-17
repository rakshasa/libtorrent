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

#ifndef LIBTORRENT_DOWNLOAD_GROUP_ENTRY_H
#define LIBTORRENT_DOWNLOAD_GROUP_ENTRY_H

#include <algorithm>
#include <vector>
#include lt_tr1_functional
#include <torrent/common.h>
#include <torrent/exceptions.h>

namespace torrent {

class choke_queue;
class PeerConnectionBase;

struct weighted_connection {
  weighted_connection(PeerConnectionBase* pcb, uint32_t w) : connection(pcb), weight(w) {}

  bool operator == (const PeerConnectionBase* pcb) { return pcb == connection; }
  bool operator != (const PeerConnectionBase* pcb) { return pcb != connection; }

  PeerConnectionBase* connection;
  uint32_t            weight;
};

// TODO: Rename to choke_entry, create an new class called group entry?
class group_entry {
public:
  typedef std::vector<weighted_connection> container_type;

  static const uint32_t unlimited = ~uint32_t();

  group_entry() : m_max_slots(unlimited), m_min_slots(0) {}

  uint32_t            size_connections() const  { return m_queued.size() + m_unchoked.size(); }

  uint32_t            max_slots() const         { return m_max_slots; }
  uint32_t            min_slots() const         { return m_min_slots; }

  void                set_max_slots(uint32_t s) { m_max_slots = s; }
  void                set_min_slots(uint32_t s) { m_min_slots = s; }

  const container_type* queued()   { return &m_queued; }
  const container_type* unchoked() { return &m_unchoked; }

protected:
  friend class choke_queue;
  friend class PeerConnectionBase;

  container_type*     mutable_queued()   { return &m_queued; }
  container_type*     mutable_unchoked() { return &m_unchoked; }

  void                connection_unchoked(PeerConnectionBase* pcb);
  void                connection_choked(PeerConnectionBase* pcb);

  void                connection_queued(PeerConnectionBase* pcb);
  void                connection_unqueued(PeerConnectionBase* pcb);

private:
  uint32_t            m_max_slots;
  uint32_t            m_min_slots;

  // After a cycle the end of the vector should have the
  // highest-priority connections, and any new connections get put at
  // the back so they should always good candidates for unchoking.
  container_type      m_queued;
  container_type      m_unchoked;
};

inline void group_entry::connection_unchoked(PeerConnectionBase* pcb) {
  container_type::iterator itr = std::find_if(m_unchoked.begin(), m_unchoked.end(),
                                              std::bind(&weighted_connection::operator==, std::placeholders::_1, pcb));

  if (itr != m_unchoked.end()) throw internal_error("group_entry::connection_unchoked(pcb) failed.");

  m_unchoked.push_back(weighted_connection(pcb, uint32_t()));
}

inline void group_entry::connection_queued(PeerConnectionBase* pcb) {
  container_type::iterator itr = std::find_if(m_queued.begin(), m_queued.end(),
                                              std::bind(&weighted_connection::operator==, std::placeholders::_1, pcb));

  if (itr != m_queued.end()) throw internal_error("group_entry::connection_queued(pcb) failed.");

  m_queued.push_back(weighted_connection(pcb, uint32_t()));
}

inline void
group_entry::connection_choked(PeerConnectionBase* pcb) {
  container_type::iterator itr = std::find_if(m_unchoked.begin(), m_unchoked.end(),
                                              std::bind(&weighted_connection::operator==, std::placeholders::_1, pcb));

  if (itr == m_unchoked.end()) throw internal_error("group_entry::connection_choked(pcb) failed.");

  std::swap(*itr, m_unchoked.back());
  m_unchoked.pop_back();
}

inline void
group_entry::connection_unqueued(PeerConnectionBase* pcb) {
  container_type::iterator itr = std::find_if(m_queued.begin(), m_queued.end(),
                                              std::bind(&weighted_connection::operator==, std::placeholders::_1, pcb));

  if (itr == m_queued.end()) throw internal_error("group_entry::connection_unqueued(pcb) failed.");

  std::swap(*itr, m_queued.back());
  m_queued.pop_back();
}

}

#endif

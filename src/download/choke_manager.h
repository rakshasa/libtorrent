// libTorrent - BitTorrent library
// Copyright (C) 2005-2007, Jari Sundell
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

#ifndef LIBTORRENT_DOWNLOAD_CHOKE_MANAGER_H
#define LIBTORRENT_DOWNLOAD_CHOKE_MANAGER_H

#include <list>
#include <vector>
#include <inttypes.h>
#include <rak/functional.h>

namespace torrent {

class ChokeManagerNode;
class ConnectionList;
class PeerConnectionBase;
class ResourceManager;

class ChokeManager {
public:
  typedef rak::mem_fun1<ResourceManager, void, int>              slot_unchoke;
  typedef rak::mem_fun0<ResourceManager, unsigned int>           slot_can_unchoke;
  typedef std::mem_fun1_t<bool, PeerConnectionBase, bool>        slot_connection;

  typedef std::vector<std::pair<PeerConnectionBase*, uint32_t> > container_type;
  typedef container_type::value_type                             value_type;
  typedef container_type::iterator                               iterator;

  typedef std::pair<uint32_t, iterator>                          target_type;

  typedef void (*slot_weight)(iterator first, iterator last);

  static const int flag_unchoke_all_new = 0x1;

  static const uint32_t order_base = (1 << 30);
  static const uint32_t order_max_size = 4;
  static const uint32_t weight_size_bytes = order_max_size * sizeof(uint32_t);

  static const uint32_t unlimited = ~uint32_t();

  ChokeManager(ConnectionList* cl, int flags = 0) :
    m_connectionList(cl),
    m_flags(flags),
    m_maxUnchoked(unlimited),
    m_generousUnchokes(3),
    m_slotConnection(NULL) {}
  ~ChokeManager();
  
  bool                is_full() const                         { return !is_unlimited() && m_unchoked.size() < m_maxUnchoked; }
  bool                is_unlimited() const                    { return m_maxUnchoked == unlimited; }

  uint32_t            size_unchoked() const                   { return m_unchoked.size(); }
  uint32_t            size_queued() const                     { return m_queued.size(); }
  uint32_t            size_total() const                      { return m_queued.size() + m_unchoked.size(); }

  uint32_t            max_unchoked() const                    { return m_maxUnchoked; }
  void                set_max_unchoked(uint32_t v)            { m_maxUnchoked = v; }

  uint32_t            generous_unchokes() const               { return m_generousUnchokes; }
  void                set_generous_unchokes(uint32_t v)       { m_generousUnchokes = v; }

  void                balance();
  int                 cycle(uint32_t quota);

  // Assume interested state is already updated for the PCB and that
  // this gets called once every time the status changes.
  void                set_queued(PeerConnectionBase* pc, ChokeManagerNode* base);
  void                set_not_queued(PeerConnectionBase* pc, ChokeManagerNode* base);

  void                set_snubbed(PeerConnectionBase* pc, ChokeManagerNode* base);
  void                set_not_snubbed(PeerConnectionBase* pc, ChokeManagerNode* base);

  void                disconnected(PeerConnectionBase* pc, ChokeManagerNode* base);

  uint32_t*           choke_weight()                           { return m_chokeWeight; }
  uint32_t*           unchoke_weight()                         { return m_unchokeWeight; }

  void                set_slot_choke_weight(slot_weight s)     { m_slotChokeWeight = s; }
  void                set_slot_unchoke_weight(slot_weight s)   { m_slotUnchokeWeight = s; }

  void                set_slot_unchoke(slot_unchoke s)         { m_slotUnchoke = s; }
  void                set_slot_can_unchoke(slot_can_unchoke s) { m_slotCanUnchoke = s; }
  void                set_slot_connection(slot_connection s)   { m_slotConnection = s; }

private:
  inline uint32_t     max_alternate() const;

  uint32_t            choke_range(iterator first, iterator last, uint32_t max);
  uint32_t            unchoke_range(iterator first, iterator last, uint32_t max);

  ConnectionList*     m_connectionList;

  container_type      m_queued;
  container_type      m_unchoked;

  uint32_t            m_chokeWeight[order_max_size];
  uint32_t            m_unchokeWeight[order_max_size];

  int                 m_flags;

  uint32_t            m_maxUnchoked;
  uint32_t            m_generousUnchokes;

  slot_weight         m_slotChokeWeight;
  slot_weight         m_slotUnchokeWeight;

  slot_unchoke        m_slotUnchoke;
  slot_can_unchoke    m_slotCanUnchoke;
  slot_connection     m_slotConnection;
};

extern uint32_t weights_upload_choke[ChokeManager::order_max_size];
extern uint32_t weights_upload_unchoke[ChokeManager::order_max_size];

void calculate_upload_choke(ChokeManager::iterator first, ChokeManager::iterator last);
void calculate_upload_unchoke(ChokeManager::iterator first, ChokeManager::iterator last);

extern uint32_t weights_download_choke[ChokeManager::order_max_size];
extern uint32_t weights_download_unchoke[ChokeManager::order_max_size];

void calculate_download_choke(ChokeManager::iterator first, ChokeManager::iterator last);
void calculate_download_unchoke(ChokeManager::iterator first, ChokeManager::iterator last);

}

#endif

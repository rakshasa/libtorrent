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

#ifndef LIBTORRENT_DOWNLOAD_CHOKE_MANAGER_H
#define LIBTORRENT_DOWNLOAD_CHOKE_MANAGER_H

#include <list>
#include <inttypes.h>
#include <rak/functional.h>

#include "connection_list.h"

namespace torrent {

class ResourceManager;
class PeerConnectionBase;

class ChokeManager {
public:
  typedef rak::mem_fun1<ResourceManager, void, unsigned int> SlotChoke;
  typedef rak::mem_fun1<ResourceManager, void, unsigned int> SlotUnchoke;
  typedef rak::mem_fun0<ResourceManager, unsigned int>       SlotCanUnchoke;

  typedef std::vector<std::pair<PeerConnectionBase*, uint32_t> > container_type;
  typedef container_type::iterator                               iterator;
  typedef container_type::value_type                             value_type;

  typedef std::pair<uint32_t, iterator>                          target_type;

  typedef void (*slot_weight)(iterator first, iterator last);

  static const uint32_t order_base = (1 << 30);
  static const uint32_t order_max_size = 4;

  ChokeManager(ConnectionList* cl) :
    m_connectionList(cl),
    m_maxUnchoked(15),
    m_generousUnchokes(3) {}
  ~ChokeManager();
  
  unsigned int        currently_unchoked() const              { return m_unchoked.size(); }
  unsigned int        currently_interested() const            { return m_interested.size() + m_unchoked.size(); }

  unsigned int        max_unchoked() const                    { return m_maxUnchoked; }
  void                set_max_unchoked(unsigned int v)        { m_maxUnchoked = v; }

  unsigned int        generous_unchokes() const               { return m_generousUnchokes; }
  void                set_generous_unchokes(unsigned int v)   { m_generousUnchokes = v; }

  void                balance();
  int                 cycle(unsigned int quota);

  // Assume interested state is already updated for the PCB and that
  // this gets called once every time the status changes.
  void                set_interested(PeerConnectionBase* pc);
  void                set_not_interested(PeerConnectionBase* pc);

  void                set_snubbed(PeerConnectionBase* pc);
  void                set_not_snubbed(PeerConnectionBase* pc);

  void                disconnected(PeerConnectionBase* pc);

  uint32_t*           choke_weight()                           { return m_chokeWeight; }
  uint32_t*           unchoke_weight()                         { return m_unchokeWeight; }

  void                slot_choke_weight(slot_weight s)         { m_slotChokeWeight = s; }
  void                slot_unchoke_weight(slot_weight s)       { m_slotUnchokeWeight = s; }

  void                slot_choke(SlotChoke s)                  { m_slotChoke = s; }
  void                slot_unchoke(SlotUnchoke s)              { m_slotUnchoke = s; }
  void                slot_can_unchoke(SlotCanUnchoke s)       { m_slotCanUnchoke = s; }

private:
  inline unsigned int max_alternate() const;

  unsigned int        choke_range(iterator first, iterator last, unsigned int max);
  unsigned int        unchoke_range(iterator first, iterator last, unsigned int max);

  ConnectionList*     m_connectionList;

  container_type      m_interested;
  container_type      m_unchoked;

  uint32_t            m_chokeWeight[order_max_size];
  uint32_t            m_unchokeWeight[order_max_size];

  unsigned int        m_maxUnchoked;
  unsigned int        m_generousUnchokes;

  slot_weight         m_slotChokeWeight;
  slot_weight         m_slotUnchokeWeight;

  SlotChoke           m_slotChoke;
  SlotUnchoke         m_slotUnchoke;
  SlotCanUnchoke      m_slotCanUnchoke;
};

extern uint32_t weights_upload_choke[ChokeManager::order_max_size];
extern uint32_t weights_upload_unchoke[ChokeManager::order_max_size];

void calculate_upload_choke(ChokeManager::iterator first, ChokeManager::iterator last);
void calculate_upload_unchoke(ChokeManager::iterator first, ChokeManager::iterator last);

}

#endif

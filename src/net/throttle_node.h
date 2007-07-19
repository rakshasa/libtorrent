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

#ifndef LIBTORRENT_NET_THROTTLE_NODE_H
#define LIBTORRENT_NET_THROTTLE_NODE_H

#include <rak/functional.h>

#include "torrent/rate.h"

#include "throttle_list.h"

namespace torrent {

class PeerConnectionBase;

class ThrottleNode {
public:
  typedef ThrottleList::iterator                  iterator;
  typedef ThrottleList::const_iterator            const_iterator;
  typedef rak::mem_fun0<PeerConnectionBase, void> SlotActivate;

  ThrottleNode(uint32_t rateSpan) : m_rate(rateSpan)  { clear_quota(); }

  Rate*               rate()                          { return &m_rate; }
  const Rate*         rate() const                    { return &m_rate; }

  uint32_t            quota() const                   { return m_quota; }
  void                clear_quota()                   { m_quota = 0; }
  void                set_quota(uint32_t q)           { m_quota = q; }

  iterator            list_iterator()                 { return m_listIterator; }
  const_iterator      list_iterator() const           { return m_listIterator; }
  void                set_list_iterator(iterator itr) { m_listIterator = itr; }

  void                activate()                      { m_slotActivate(); }

  void                slot_activate(SlotActivate s)   { m_slotActivate = s; }

private:
  ThrottleNode(const ThrottleNode&);
  void operator = (const ThrottleNode&);

  uint32_t            m_quota;
  iterator            m_listIterator;

  Rate                m_rate;
  SlotActivate        m_slotActivate;
};

}

#endif

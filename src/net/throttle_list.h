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

#ifndef LIBTORRENT_NET_THROTTLE_LIST_H
#define LIBTORRENT_NET_THROTTLE_LIST_H

#include <list>

#include "torrent/rate.h"
#include "throttle_node.h"

namespace torrent {

class ThrottleList : private std::list<ThrottleNode> {
public:
  typedef std::list<ThrottleNode> Base;

  using Base::iterator;
  using Base::reverse_iterator;
  using Base::clear;
  using Base::size;

  using Base::begin;
  using Base::end;
  using Base::rbegin;
  using Base::rend;

  static const uint32_t min_trigger_activate = 2048;

  ThrottleList();

  bool                is_active(iterator itr) const;
  bool                is_inactive(iterator itr) const;

  // When disabled all nodes are active at all times.
  void                enable();
  void                disable();

  void                update_quota(uint32_t quota);

  uint32_t            size() const                   { return m_size; }

  // Propably some functions for controlling how much we allocate,
  // need to be able to activate etc.

  uint32_t            outstanding_quota() const      { return m_outstandingQuota; }
  uint32_t            unallocated_quota() const      { return m_unallocatedQuota; }

  uint32_t            min_chunk_size() const         { return m_minChunkSize; }
  void                set_min_chunk_size(uint32_t v) { m_minChunkSize = v; }

  uint32_t            node_quota(iterator itr);
  void                node_used(iterator itr, uint32_t used);
  void                node_deactivate(iterator itr);

  const Rate&         rate_slow() const              { return m_rateSlow; }

  // It is asumed that inserted nodes are currently active. It won't
  // matter if they do not get any initial quota as a later activation
  // of an active node should be safe.
  iterator            insert(ThrottleNode::SlotActivate s);
  void                erase(iterator itr);

private:
  inline bool         can_activate() const;

  inline uint32_t     allocate_quota();

  bool                m_enabled;
  uint32_t            m_size;

  uint32_t            m_outstandingQuota;
  uint32_t            m_unallocatedQuota;

  uint32_t            m_minChunkSize;

  Rate                m_rateSlow;

  // [m_splitActive,end> contains nodes that are inactive and need
  // more quote, sorted from the most urgent
  // node. [begin,m_splitActive> holds nodes with a large enough quota
  // to transmit, but are blocking. These are sorted from the longest
  // blocking node.
  iterator            m_splitActive;
};

}

#endif

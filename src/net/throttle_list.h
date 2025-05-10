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

#ifndef LIBTORRENT_NET_THROTTLE_LIST_H
#define LIBTORRENT_NET_THROTTLE_LIST_H

#include <list>

#include "torrent/rate.h"

// To allow conditional compilation depending on whether this patch is applied or not.
#define LIBTORRENT_CUSTOM_THROTTLES 1

namespace torrent {

class ThrottleNode;

class ThrottleList : private std::list<ThrottleNode*> {
public:
  using base_type = std::list<ThrottleNode*>;

  using base_type::iterator;
  using base_type::const_iterator;
  using base_type::reverse_iterator;
  using base_type::clear;
  using base_type::size;

  using base_type::begin;
  using base_type::end;
  using base_type::rbegin;
  using base_type::rend;

  ThrottleList();

  bool                is_enabled() const             { return m_enabled; }

  bool                is_active(const ThrottleNode* node) const;
  bool                is_inactive(const ThrottleNode* node) const;

  bool                is_throttled(const ThrottleNode* node) const;

  // When disabled all nodes are active at all times.
  void                enable();
  void                disable();

  // Returns the amount of quota used. May be negative if it had unused
  // quota left over from the last call that was more than is now allowed.
  int32_t             update_quota(uint32_t quota);

  uint32_t            size() const                   { return m_size; }

  uint32_t            outstanding_quota() const      { return m_outstandingQuota; }
  uint32_t            unallocated_quota() const      { return m_unallocatedQuota; }

  uint32_t            min_chunk_size() const         { return m_minChunkSize; }
  void                set_min_chunk_size(uint32_t v) { m_minChunkSize = v; }

  uint32_t            max_chunk_size() const         { return m_maxChunkSize; }
  void                set_max_chunk_size(uint32_t v) { m_maxChunkSize = v; }

  uint32_t            node_quota(ThrottleNode* node);
  uint32_t            node_used(ThrottleNode* node, uint32_t used);  // both node_used functions
  uint32_t            node_used_unthrottled(uint32_t used);          // return the "used" argument
  void                node_deactivate(ThrottleNode* node);

  const Rate*         rate_slow() const              { return &m_rateSlow; }

  void                add_rate(uint32_t used);
  uint32_t            rate_added()                   { uint32_t v = m_rateAdded; m_rateAdded = 0; return v; }

  // It is asumed that inserted nodes are currently active. It won't
  // matter if they do not get any initial quota as a later activation
  // of an active node should be safe.
  void                insert(ThrottleNode* node);
  void                erase(ThrottleNode* node);

private:
  inline void         allocate_quota(ThrottleNode* node);

  bool                m_enabled{false};
  uint32_t            m_size{0};

  uint32_t            m_outstandingQuota{0};
  uint32_t            m_unallocatedQuota{0};
  uint32_t            m_unusedUnthrottledQuota{0};

  uint32_t            m_rateAdded{0};

  uint32_t            m_minChunkSize;
  uint32_t            m_maxChunkSize;

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

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

#include "config.h"

#include <algorithm>
#include <limits>
#include <torrent/exceptions.h>

#include "throttle_list.h"
#include "throttle_node.h"

namespace torrent {

ThrottleList::ThrottleList() :
  m_enabled(false),
  m_size(0),
  m_outstandingQuota(0),
  m_unallocatedQuota(0),
  m_unusedUnthrottledQuota(0),
  m_rateAdded(0),

  m_minChunkSize(2 << 10),
  m_maxChunkSize(16 << 10),

  m_rateSlow(60),
  m_splitActive(end()) {
}

bool
ThrottleList::is_active(const ThrottleNode* node) const {
  return std::find(begin(), (const_iterator)m_splitActive, node) != m_splitActive;
}

bool
ThrottleList::is_inactive(const ThrottleNode* node) const {
  return std::find((const_iterator)m_splitActive, end(), node) != end();
}

bool
ThrottleList::is_throttled(const ThrottleNode* node) const {
  return node->list_iterator() != end();
}

// The quota already present in the node is preserved and unallocated
// quota is transferred to the node. The node's quota will be less
// than or equal to 'm_minChunkSize'.
inline void
ThrottleList::allocate_quota(ThrottleNode* node) {
  if (node->quota() >= m_minChunkSize)
    return;

  int quota = std::min(m_maxChunkSize - node->quota(), m_unallocatedQuota);

  node->set_quota(node->quota() + quota);
  m_outstandingQuota += quota;
  m_unallocatedQuota -= quota;
}

void
ThrottleList::enable() {
  if (m_enabled)
    return;

  m_enabled = true;

  if (!empty() && m_splitActive == begin())
    throw internal_error("ThrottleList::enable() m_splitActive is invalid.");
}

void
ThrottleList::disable() {
  if (!m_enabled)
    return;

  m_enabled = false;

  m_outstandingQuota = 0;
  m_unallocatedQuota = 0;
  m_unusedUnthrottledQuota = 0;

  std::for_each(begin(), end(), std::mem_fn(&ThrottleNode::clear_quota));
  std::for_each(m_splitActive, end(), std::mem_fn(&ThrottleNode::activate));

  m_splitActive = end();
}

int32_t
ThrottleList::update_quota(uint32_t quota) {
  if (!m_enabled)
    throw internal_error("ThrottleList::update_quota(...) called but the object is not enabled.");

  // Distribute new quota to unthrottled quota first, and use
  // left-over unthrottled quota from last turn to be allocated
  // to throttled nodes this turn.
  // When distributing, we include the unallocated quota from the
  // previous turn. This will ensure that quota that was reclaimed
  // will have a chance of being used, even by those nodes that were
  // deactivated.
  m_unallocatedQuota += m_unusedUnthrottledQuota;
  m_unusedUnthrottledQuota = quota;

  // Add remaining to the next, even when less than activate border.
  while (m_splitActive != end()) {
    allocate_quota(*m_splitActive);

    if ((*m_splitActive)->quota() < m_minChunkSize)
      break;

    (*m_splitActive)->activate();
    m_splitActive++;
  }

  // Use 'quota' as an upper bound to avoid accumulating unused quota
  // over time. Return actually used amount of quota.
  int32_t used = quota;

  if (m_unallocatedQuota > quota) {
    used -= m_unallocatedQuota - quota;
    m_unallocatedQuota = quota;
  }

  return used;
}

uint32_t
ThrottleList::node_quota(ThrottleNode* node) {
  if (!m_enabled) {
    // Returns max for signed integer to ensure we don't overflow
    // claculations.
    return std::numeric_limits<int32_t>::max();

  } else if (!is_active(node)) {
    throw internal_error(is_inactive(node) ?
                         "ThrottleList::node_quota(...) called on an inactive node." :
                         "ThrottleList::node_quota(...) could not find node.");

  } else if (node->quota() + m_unallocatedQuota >= m_minChunkSize) {
    return node->quota() + m_unallocatedQuota;

  } else {
    return 0;
  }
}

void
ThrottleList::add_rate(uint32_t used) {
  m_rateSlow.insert(used);
  m_rateAdded += used;
}

uint32_t
ThrottleList::node_used(ThrottleNode* node, uint32_t used) {
  add_rate(used);
  node->rate()->insert(used);

  if (used == 0 || !m_enabled || node->list_iterator() == end())
    return used;

  uint32_t quota = std::min(used, node->quota());

  if (quota > m_outstandingQuota)
    throw internal_error("ThrottleList::node_used(...) used too much quota.");

  node->set_quota(node->quota() - quota);
  m_outstandingQuota -= quota;
  m_unallocatedQuota -= std::min(used - quota, m_unallocatedQuota);

  return used;
}

uint32_t
ThrottleList::node_used_unthrottled(uint32_t used) {
  add_rate(used);

  // Use what we can from the unthrottled quota,
  // if node used too much borrow from throttled quota.
  uint32_t avail = std::min(used, m_unusedUnthrottledQuota);
  m_unusedUnthrottledQuota -= avail;
  m_unallocatedQuota -= std::min(used - avail, m_unallocatedQuota);

  return used;
}

void
ThrottleList::node_deactivate(ThrottleNode* node) {
  if (!is_active(node))
    throw internal_error(is_inactive(node) ?
                         "ThrottleList::node_deactivate(...) called on an inactive node." :
                         "ThrottleList::node_deactivate(...) could not find node.");

  base_type::splice(end(), *this, node->list_iterator());

  if (m_splitActive == end())
    m_splitActive = node->list_iterator();
}

void
ThrottleList::insert(ThrottleNode* node) {
  if (node->list_iterator() != end())
    return;

  if (!m_enabled) {
    // Add to waiting queue.
    node->set_list_iterator(base_type::insert(end(), node));
    node->clear_quota();

  } else {
    // Add before the active split, so if we only need to decrement
    // m_splitActive to change the queue it is in.
    node->set_list_iterator(base_type::insert(m_splitActive, node));
    allocate_quota(node);
  }

  m_size++;
}

void
ThrottleList::erase(ThrottleNode* node) {
  if (node->list_iterator() == end())
    return;

  if (m_size == 0)
    throw internal_error("ThrottleList::erase(...) called on an empty list.");

  // Do we need an if-statement here?
  if (node->quota() != 0) {
    if (node->quota() > m_outstandingQuota)
      throw internal_error("ThrottleList::erase(...) node->quota() > m_outstandingQuota.");

    m_outstandingQuota -= node->quota();
    m_unallocatedQuota += node->quota();
  }

  if (node->list_iterator() == m_splitActive)
    m_splitActive = base_type::erase(node->list_iterator());
  else
    base_type::erase(node->list_iterator());

  node->clear_quota();
  node->set_list_iterator(end());
  m_size--;
}

}

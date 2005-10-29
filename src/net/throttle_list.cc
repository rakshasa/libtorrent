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

#include "config.h"

#include <algorithm>
#include <limits>
#include <torrent/exceptions.h>

#include "throttle_list.h"

namespace torrent {

inline bool
ThrottleList::can_activate() const {
  return m_unallocatedQuota >= (1 << 14);
}

inline uint32_t
ThrottleList::allocate_quota() {
  int quota = (1 << 14);

  m_outstandingQuota += quota;
  m_unallocatedQuota -= quota;

  return quota;
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

  std::for_each(begin(), end(), std::mem_fun_ref(&ThrottleNode::clear_quota));
  std::for_each(m_splitActive, end(), std::mem_fun_ref(&ThrottleNode::activate));

  m_splitActive = end();
}

void
ThrottleList::update_quota(uint32_t quota) {
  if (!m_enabled)
    throw internal_error("ThrottleList::update_quota(...) called but the object is not enabled.");

  m_unallocatedQuota = quota;

  while (m_splitActive != end() && can_activate()) {
    iterator itr = m_splitActive++;

    itr->set_quota(itr->quota() + allocate_quota());
    itr->activate();
  }
}

uint32_t
ThrottleList::node_quota(iterator itr) {
  if (!m_enabled) {
    return std::numeric_limits<uint32_t>::max();

  } else if (itr->quota() >= 2048) {
    return itr->quota();

  } else {
    // Put into waiting queue. Consider doing this in a seperate
    // function called bu the peer?
    //
    // We can't really do it this way, as it will cause a node's position to be reset
    if (itr != m_splitActive)
      Base::splice(end(), *this, itr);

    if (m_splitActive == end())
      m_splitActive = itr;

    return 0;
  }
}

void
ThrottleList::node_used(iterator itr, uint32_t used) {
  m_rateSlow.insert(used);

  if (itr == end())
    //throw internal_error("ThrottleList::node_used(...) received ..");
    return;

  if (!m_enabled)
    return;

  // Currently check for end and ignore.
  if (used > m_outstandingQuota || used > itr->quota())
    throw internal_error("ThrottleList::node_used(...) 'used' overflow..");

  m_outstandingQuota -= used;
  itr->set_quota(itr->quota() - used);

  // Add quota if available. Temporary solution. Think about it.
  if (itr->quota() < (1 << 14) && can_activate())
    itr->set_quota(itr->quota() + allocate_quota());
}

ThrottleList::iterator
ThrottleList::insert(ThrottleNode::SlotActivate s) {
  iterator itr;

  if (!m_enabled) {
    // Add to waiting queue.
    itr = Base::insert(end(), ThrottleNode());

  } else if (can_activate()) {
    // Add before the active split, so if we only need to decrement
    // m_splitActive to change the queue it is in.
    itr = Base::insert(m_splitActive, ThrottleNode());
    itr->set_quota(allocate_quota());

  } else {
    itr = Base::insert(end(), ThrottleNode());

    if (m_splitActive == end())
      m_splitActive = itr;
  }

  itr->slot_activate(s);
  return itr;
}

void
ThrottleList::erase(iterator itr) {
  // Do we need an if-statement here?
  if (itr->quota() != 0) {
    if (itr->quota() > m_outstandingQuota)
      throw internal_error("ThrottleList::erase(...) itr->quota() > m_outstandingQuota.");

    m_outstandingQuota -= itr->quota();
    m_unallocatedQuota += itr->quota();
  }

  if (itr == m_splitActive)
    m_splitActive = Base::erase(itr);
  else
    Base::erase(itr);
}

}

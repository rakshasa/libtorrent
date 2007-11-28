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

#include "config.h"

#include "torrent/exceptions.h"

#include "dht_bucket.h"
#include "dht_node.h"

namespace torrent {

DhtBucket::DhtBucket(const HashString& begin, const HashString& end) :
  m_parent(NULL),
  m_child(NULL),

  m_lastChanged(cachedTime.seconds()),

  m_good(0),
  m_bad(0),

  m_begin(begin),
  m_end(end) {

  reserve(num_nodes);
}

void
DhtBucket::add_node(DhtNode* n) {
  push_back(n);
  touch();

  if (n->is_good())
    m_good++;
  else if (n->is_bad())
    m_bad++;
}

void
DhtBucket::remove_node(DhtNode* n) {
  iterator itr = std::find_if(begin(), end(), std::bind2nd(std::equal_to<DhtNode*>(), n));
  if (itr == end())
    throw internal_error("DhtBucket::remove_node called for node not in bucket.");

  erase(itr);

  if (n->is_good())
    m_good--;
  else if (n->is_bad())
    m_bad--;
}

void
DhtBucket::count() {
  m_good = std::count_if(begin(), end(), std::mem_fun(&DhtNode::is_good));
  m_bad = std::count_if(begin(), end(), std::mem_fun(&DhtNode::is_bad));
}

// Called every 15 minutes for housekeeping.
void
DhtBucket::update() {
  // For now we only update the counts after some nodes have become bad
  // due to prolonged inactivity.
  count();
}

DhtBucket::iterator
DhtBucket::find_replacement_candidate(bool onlyOldest) {
  iterator oldest = end();
  unsigned int oldestTime = std::numeric_limits<unsigned int>::max();

  for (iterator itr = begin(); itr != end(); ++itr) {
    if ((*itr)->is_bad() && !onlyOldest)
      return itr;

    if ((*itr)->last_seen() < oldestTime) {
      oldestTime = (*itr)->last_seen();
      oldest = itr;
    }
  }

  return oldest;
}

void
DhtBucket::get_mid_point(HashString* middle) const {
  *middle = m_end;

  for (unsigned int i=0; i<m_begin.size(); i++)
    if (m_begin[i] != m_end[i]) {
      (*middle)[i] = ((uint8_t)m_begin[i] + (uint8_t)m_end[i]) / 2;
      break;
    }
}

void
DhtBucket::get_random_id(HashString* rand_id) const {

  // Generate a random ID between m_begin and m_end.
  // Since m_end - m_begin = 2^n - 1, we can do a bitwise AND operation.
  for (unsigned int i=0; i<(*rand_id).size(); i++)
    (*rand_id)[i] = m_begin[i] + (random() & (m_end[i] - m_begin[i]));

#ifdef USE_EXTRA_DEBUG
  if (!is_in_range(*rand_id))
    throw internal_error("DhtBucket::get_random_id generated an out-of-range ID.");
#endif
}

DhtBucket*
DhtBucket::split(const HashString& id) {
  HashString mid_range;
  get_mid_point(&mid_range);

  DhtBucket* other = new DhtBucket(m_begin, mid_range);

  // Set m_begin = mid_range + 1
  int carry = 1;
  for (unsigned int i = mid_range.size(); i>0; i--) {
    unsigned int sum = (uint8_t)mid_range[i-1] + carry;
    m_begin[i-1] = (uint8_t)sum;
    carry = sum >> 8;
  }

  // Move nodes over to other bucket if they fall in its range, then
  // delete them from this one.
  iterator split = std::partition(begin(), end(), std::bind2nd(std::mem_fun(&DhtNode::is_in_range), this));
  other->insert(other->end(), split, end());
  std::for_each(other->begin(), other->end(), std::bind2nd(std::mem_fun(&DhtNode::set_bucket), other));
  erase(split, end());

  other->set_time(m_lastChanged);
  other->count();

  count();

  // Maintain child (adjacent narrower bucket) and parent (adjacent wider bucket)
  // so that given router ID is in child.
  if (other->is_in_range(id)) {
    // Make other become our new child.
    m_child = other;
    other->m_parent = this;

  } else {
    // We become other's child, other becomes our parent's child.
    if (parent() != NULL) {
      parent()->m_child = other;
      other->m_parent = parent();
    }

    m_parent = other;
    other->m_child = this;
  }

  return other;
}

}

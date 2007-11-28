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

#ifndef LIBTORRENT_DHT_NODE_H
#define LIBTORRENT_DHT_NODE_H

#include "globals.h"

#include <rak/socket_address.h>

#include "torrent/hash_string.h"

#include "dht_bucket.h"

namespace torrent {

class DhtBucket;

class DhtNode : public HashString {
  friend class DhtSearch;

public:
  // A node is considered bad if it failed to reply to this many queries.
  static const unsigned int max_failed_replies = 5;

  DhtNode(const HashString& id, const rak::socket_address* sa);
  DhtNode(const std::string& id, const Object& cache);

  const HashString&           id() const                 { return *this; }
  const rak::socket_address*  address() const            { return &m_socketAddress; }
  void                        set_address(const rak::socket_address* sa) { m_socketAddress = *sa; }

  // For determining node quality.
  unsigned int                last_seen() const          { return m_lastSeen; }
  unsigned int                age() const                { return cachedTime.seconds() - m_lastSeen; }
  bool                        is_good() const            { return m_recentlyActive; }
  bool                        is_questionable() const    { return !m_recentlyActive; }
  bool                        is_bad() const             { return m_recentlyInactive >= max_failed_replies; };
  bool                        is_active() const          { return m_lastSeen; }

  // Update is called once every 15 minutes.
  void                        update()                   { m_recentlyActive = age() < 15 * 60; }
                                                         
  // Called when node replies to us, queries us, or fails to reply.
  void                        replied()                  { set_good(); }
  void                        queried()                  { if (m_lastSeen) set_good(); }
  void                        inactive();

  DhtBucket*                  bucket() const             { return m_bucket; }
  DhtBucket*                  set_bucket(DhtBucket* b)   { m_bucket = b; return b; }

  bool                        is_in_range(const DhtBucket* b) { return b->is_in_range(*this); }

  // Store compact node information (26 bytes address, port and ID) in the given
  // buffer and return pointer to end of stored information.
  char*                       store_compact(char* buffer) const;

  // Store node cache in the given container object and return it.
  Object*                     store_cache(Object* container) const;

private:
  DhtNode();

  void                        set_good();
  void                        set_bad();

  rak::socket_address m_socketAddress;
  unsigned int        m_lastSeen;
  bool                m_recentlyActive;
  unsigned int        m_recentlyInactive;
  DhtBucket*          m_bucket;
};

inline void
DhtNode::set_good() {
  if (m_bucket != NULL && !is_good())
    m_bucket->node_now_good(is_bad());

  m_lastSeen = cachedTime.seconds();
  m_recentlyInactive = 0;
  m_recentlyActive = true;
}

inline void
DhtNode::set_bad() {
  if (m_bucket != NULL && !is_bad())
    m_bucket->node_now_bad(is_good());

  m_recentlyInactive = max_failed_replies;
  m_recentlyActive = false;
}

inline void
DhtNode::inactive() {
  if (m_recentlyInactive + 1 == max_failed_replies)
    set_bad();
  else 
    m_recentlyInactive++;
}

}

#endif

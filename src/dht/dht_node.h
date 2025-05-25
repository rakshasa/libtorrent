#ifndef LIBTORRENT_DHT_NODE_H
#define LIBTORRENT_DHT_NODE_H

#include "globals.h"

#include <rak/socket_address.h>

#include "torrent/hash_string.h"
#include "torrent/object_raw_bencode.h"

#include "dht_bucket.h"

namespace torrent {

class DhtBucket;

class DhtNode : public HashString {
  friend class DhtSearch;

public:
  // A node is considered bad if it failed to reply to this many queries.
  static constexpr unsigned int max_failed_replies = 5;

  DhtNode(const HashString& id, const rak::socket_address* sa);
  DhtNode(const std::string& id, const Object& cache);
  ~DhtNode() = default;
  DhtNode(const DhtNode&) = delete;
  DhtNode& operator=(const DhtNode&) = delete;

  const HashString&           id() const                 { return *this; }
  raw_string                  id_raw_string() const      { return raw_string(data(), size_data); }
  const rak::socket_address*  address() const            { return &m_socketAddress; }
  void                        set_address(const rak::socket_address* sa) { m_socketAddress = *sa; }

  // For determining node quality.
  unsigned int                last_seen() const          { return m_lastSeen; }
  unsigned int                age() const                { return this_thread::cached_seconds().count() - m_lastSeen; }
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
  void                        set_good();
  void                        set_bad();

  rak::socket_address m_socketAddress;
  unsigned int        m_lastSeen;
  bool                m_recentlyActive{};
  unsigned int        m_recentlyInactive{};
  DhtBucket*          m_bucket{};
};

inline void
DhtNode::set_good() {
  if (m_bucket != NULL && !is_good())
    m_bucket->node_now_good(is_bad());

  m_lastSeen = this_thread::cached_seconds().count();
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

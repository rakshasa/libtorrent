#ifndef LIBTORRENT_DHT_NODE_H
#define LIBTORRENT_DHT_NODE_H

#include "dht/dht_bucket.h"
#include "torrent/hash_string.h"
#include "torrent/object_raw_bencode.h"
#include "torrent/net/types.h"

namespace torrent {

class DhtBucket;

class DhtNode : public HashString {
public:
  friend class DhtSearch;

  // A node is considered bad if it failed to reply to this many queries.
  static constexpr unsigned int max_failed_replies = 5;

  DhtNode(const HashString& id, const sockaddr* sa);
  DhtNode(const std::string& id, const Object& cache);
  ~DhtNode() = default;

  const HashString&   id() const                 { return *this; }
  raw_string          id_raw_string() const      { return raw_string(data(), size_data); }

  const sockaddr*     address() const            { return m_socket_address.get(); }
  void                set_address(const sockaddr* sa);

  // For determining node quality.
  unsigned int        last_seen() const          { return m_last_seen; }
  unsigned int        age() const                { return this_thread::cached_seconds().count() - m_last_seen; }
  bool                is_good() const            { return m_recently_active; }
  bool                is_questionable() const    { return !m_recently_active; }
  bool                is_bad() const             { return m_recently_inactive >= max_failed_replies; }
  bool                is_active() const          { return m_last_seen; }

  // Update is called once every 15 minutes.
  void                update()                   { m_recently_active = age() < 15 * 60; }

  // Called when node replies to us, queries us, or fails to reply.
  void                replied()                  { set_good(); }
  void                queried()                  { if (m_last_seen) set_good(); }
  void                inactive();

  DhtBucket*          bucket() const             { return m_bucket; }
  DhtBucket*          set_bucket(DhtBucket* b)   { m_bucket = b; return b; }

  bool                is_in_range(const DhtBucket* b) { return b->is_in_range(*this); }

  // Store compact node information (26 bytes address, port and ID) in the given
  // buffer and return pointer to end of stored information.
  char*               store_compact(char* buffer) const;

  // Store node cache in the given container object and return it.
  Object*             store_cache(Object* container) const;

private:
  DhtNode(const DhtNode&) = delete;
  DhtNode& operator=(const DhtNode&) = delete;

  void                set_good();
  void                set_bad();

  sa_unique_ptr       m_socket_address;
  unsigned int        m_last_seen{};
  bool                m_recently_active{};
  unsigned int        m_recently_inactive{};
  DhtBucket*          m_bucket{};
};

inline void
DhtNode::set_good() {
  if (m_bucket != NULL && !is_good())
    m_bucket->node_now_good(is_bad());

  m_last_seen = this_thread::cached_seconds().count();
  m_recently_inactive = 0;
  m_recently_active = true;
}

inline void
DhtNode::set_bad() {
  if (m_bucket != NULL && !is_bad())
    m_bucket->node_now_bad(is_good());

  m_recently_inactive = max_failed_replies;
  m_recently_active = false;
}

inline void
DhtNode::inactive() {
  if (m_recently_inactive + 1 == max_failed_replies)
    set_bad();
  else
    m_recently_inactive++;
}

} // namespace torrent

#endif

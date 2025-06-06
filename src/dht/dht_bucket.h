#ifndef LIBTORRENT_DHT_BUCKET_H
#define LIBTORRENT_DHT_BUCKET_H

#include <list>
#include <vector>

#include "torrent/hash_string.h"
#include "torrent/object_raw_bencode.h"

namespace torrent {

class DhtNode;

// A container holding a small number of nodes that fall in a given binary
// partition of the 160-bit ID space (i.e. the range ID1..ID2 where ID2-ID1+1 is
// a power of 2.)
class DhtBucket : private std::vector<DhtNode*> {
public:
  static constexpr unsigned int num_nodes = 8;

  using base_type = std::vector<DhtNode*>;

  using base_type::const_iterator;
  using base_type::iterator;

  using base_type::begin;
  using base_type::end;
  using base_type::size;
  using base_type::empty;

  DhtBucket(const HashString& begin, const HashString& end);

  // Add new node. Does NOT set node's bucket automatically (to allow adding a
  // node to multiple buckets, with only one "main" bucket.)
  void                add_node(DhtNode* n);

  void                remove_node(DhtNode* n);

  // Bucket's ID range functions.
  const HashString&   id_range_begin() const                  { return m_begin; }
  HashString&         id_range_begin()                        { return m_begin; }
  const HashString&   id_range_end() const                    { return m_end; }

  bool                is_in_range(const HashString& id) const { return m_begin <= id && id <= m_end; }

  // Find middle or random ID in bucket.
  void                get_mid_point(HashString* middle) const;
  void                get_random_id(HashString* rand_id) const;

  // Node counts and bucket stats.
  bool                is_full() const                         { return size() >= num_nodes; }
  bool                has_space() const                       { return !is_full() || num_bad() > 0; }
  unsigned int        num_good() const                        { return m_good; }
  unsigned int        num_bad() const                         { return m_bad; }

  unsigned int        age() const                             { return this_thread::cached_seconds().count() - m_last_changed; }
  void                touch()                                 { m_last_changed = this_thread::cached_seconds().count(); }
  void                set_time(int32_t time)                  { m_last_changed = time; }

  // Called every 15 minutes after updating nodes.
  void                update();

  // Return candidate for replacement (a bad node or the oldest node); may
  // return end() unless has_space() is true.
  iterator            find_replacement_candidate(bool onlyOldest = false);

  // Split the bucket in two and redistribute nodes. Returned bucket is the
  // lower half, "this" bucket keeps the upper half.   Sets parent/child so
  // that the bucket the given ID falls in is the child.
  DhtBucket*          split(const HashString& id);

  // Parent and child buckets.  Parent is the adjacent bucket with double the
  // ID width, child the adjacent bucket with half the width (except the very
  // last child which has the same width.)
  DhtBucket*          parent() const                          { return m_parent; }
  DhtBucket*          child() const                           { return m_child; }

  // Return a full bucket's worth of compact node data. If this bucket is not
  // full, it uses nodes from the child/parent buckets until we have enough.
  raw_string          full_bucket();

  // Called by the DhtNode on its bucket to update good/bad node counts.
  void                node_now_good(bool was_bad);
  void                node_now_bad(bool was_good);

private:
  void                count();

  void                build_full_cache();

  DhtBucket*          m_parent{};
  DhtBucket*          m_child{};

  int64_t             m_last_changed{this_thread::cached_seconds().count()};

  unsigned int        m_good{0};
  unsigned int        m_bad{0};

  size_t              m_fullCacheLength{0};

  // These are 40 bytes together, so might as well put them last.
  // m_end is const because it is used as key for the DhtRouter routing table
  // map, which would be inconsistent if m_end were changed carelessly.
  HashString          m_begin;
  HashString          m_end;

  char                m_fullCache[num_nodes * 26];
};

// Helper class to recursively follow a chain of buckets.  It first recurses
// into the bucket's children since they are by definition closer to the bucket,
// then continues with the bucket's parents.
class DhtBucketChain {
public:
  DhtBucketChain(const DhtBucket* b) : m_restart(b), m_cur(b) { }

  const DhtBucket*          bucket()                          { return m_cur; }
  const DhtBucket*          next();

private:
  const DhtBucket*          m_restart;
  const DhtBucket*          m_cur;
};

inline void
DhtBucket::node_now_good(bool was_bad) {
  m_bad -= was_bad;
  m_good++;
}

inline void
DhtBucket::node_now_bad(bool was_good) {
  m_good -= was_good;
  m_bad++;
}

inline raw_string
DhtBucket::full_bucket() {
  if (!m_fullCacheLength)
    build_full_cache();

  return raw_string(m_fullCache, m_fullCacheLength);
}

inline const DhtBucket*
DhtBucketChain::next() {
  // m_restart is clear when we're done recursing into the children and
  // follow the parents instead.
  if (m_restart == NULL) {
    m_cur = m_cur->parent();

  } else {
    m_cur = m_cur->child();

    if (m_cur == NULL) {
      m_cur = m_restart->parent();
      m_restart = NULL;
    }
  }

  return m_cur;
}

} // namespace torrent

#endif

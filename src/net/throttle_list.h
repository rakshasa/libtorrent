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

  uint32_t            node_quota(ThrottleNode* node) const;
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

  uint32_t            m_minChunkSize{2 << 10};
  uint32_t            m_maxChunkSize{16 << 10};

  Rate                m_rateSlow{60};

  // [m_splitActive,end> contains nodes that are inactive and need
  // more quote, sorted from the most urgent
  // node. [begin,m_splitActive> holds nodes with a large enough quota
  // to transmit, but are blocking. These are sorted from the longest
  // blocking node.
  iterator            m_splitActive{end()};
};

} // namespace torrent

#endif

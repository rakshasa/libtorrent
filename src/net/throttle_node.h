#ifndef LIBTORRENT_NET_THROTTLE_NODE_H
#define LIBTORRENT_NET_THROTTLE_NODE_H

#include <functional>

#include "torrent/rate.h"

#include "throttle_list.h"

namespace torrent {

class ThrottleNode {
public:
  using iterator       = ThrottleList::iterator;
  using const_iterator = ThrottleList::const_iterator;

  using slot_void = std::function<void()>;

  // Span, in seconds, of the second rate counter below.
  static constexpr uint32_t rate_recent_span = 5;

  ThrottleNode(uint32_t rateSpan) : m_rate(rateSpan)  { clear_quota(); }
  ~ThrottleNode() = default;

  Rate*               rate()                          { return &m_rate; }
  const Rate*         rate() const                    { return &m_rate; }

  // Rate::rate() divides by the counter's whole span rather than by the time
  // it has actually been running, so a counter reads a fraction of the true
  // rate until it is span-seconds old: the 30s node below reads a thirtieth of
  // reality one second into a connection, a sixth of it after five. Consumers
  // that need a recent reading rather than a smooth one use this counter
  // instead; both are fed from the same place, so it costs one deque of at
  // most rate_recent_span entries per node.
  Rate*               rate_recent()                   { return &m_rate_recent; }
  const Rate*         rate_recent() const             { return &m_rate_recent; }

  uint32_t            quota() const                   { return m_quota; }
  void                clear_quota()                   { m_quota = 0; }
  void                set_quota(uint32_t q)           { m_quota = q; }

  iterator            list_iterator()                 { return m_listIterator; }
  const_iterator      list_iterator() const           { return m_listIterator; }
  void                set_list_iterator(iterator itr) { m_listIterator = itr; }

  void                activate()                      { if (m_slot_activate) m_slot_activate(); }

  slot_void&          slot_activate()                 { return m_slot_activate; }

private:
  ThrottleNode(const ThrottleNode&) = delete;
  ThrottleNode& operator=(const ThrottleNode&) = delete;

  uint32_t            m_quota;
  iterator            m_listIterator;

  Rate                m_rate;
  Rate                m_rate_recent{rate_recent_span};
  slot_void           m_slot_activate;
};

} // namespace torrent

#endif

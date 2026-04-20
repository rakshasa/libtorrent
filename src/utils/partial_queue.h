#ifndef LIBTORRENT_UTILS_PARTIAL_QUEUE_H
#define LIBTORRENT_UTILS_PARTIAL_QUEUE_H

#include <array>
#include <cinttypes>
#include <cstring>
#include <memory>
#include <stdexcept>

namespace torrent::utils {

// First step, don't allow overflowing to the next layer. Only disable
// the above layers for now.

// We also include 0 in a single layer as some chunk may be available
// only through seeders.

class PartialQueue {
public:
  typedef uint8_t                         key_type;
  typedef uint32_t                        mapped_type;
  typedef uint16_t                        size_type;
  typedef std::pair<size_type, size_type> size_pair_type;

  static constexpr size_type num_layers = 8;

  PartialQueue() = default;
  ~PartialQueue() = default;

  bool                is_full() const                         { return m_ceiling == 0; }
  bool                is_layer_full(size_type l) const        { return m_layers[l].second >= m_max_layer_size; }

  bool                is_enabled() const                      { return m_data != NULL; }

  // Add check to see if we can add more. Also make it possible to
  // check how full we are in the lower parts so the caller knows when
  // he can stop searching.
  //
  // Though propably not needed, as we must continue til the first
  // layer is full.

  size_type           max_size() const                        { return m_max_layer_size * num_layers; }
  size_type           max_layer_size() const                  { return m_max_layer_size; }

  // Must be less that or equal to (max size_type) / num_layers.
  void                enable(size_type ls);
  void                disable();

  void                clear();

  // Safe to call while pop'ing and it will not reuse pop'ed indices
  // so it is guaranteed to reach max_size at some point. This will
  // ensure that the user needs to refill with new data at regular
  // intervals.
  bool                insert(key_type key, mapped_type value);

  // Only call this when pop'ing as it moves the index.
  bool                prepare_pop();

  mapped_type         pop();

private:
  PartialQueue(const PartialQueue&) = delete;
  PartialQueue& operator=(const PartialQueue&) = delete;

  static size_type    ceiling(size_type layer)                { return (2 << layer) - 1; }

  std::unique_ptr<mapped_type[]> m_data;

  size_type           m_max_layer_size{};
  size_type           m_index{};
  size_type           m_ceiling{};

  std::array<size_pair_type, num_layers> m_layers;
};

inline void
PartialQueue::enable(size_type ls) {
  if (ls == 0)
    throw std::logic_error("PartialQueue::enable(...) ls == 0.");

  m_data.reset(new mapped_type[ls * num_layers]);

  m_max_layer_size = ls;
}

inline void
PartialQueue::disable() {
  m_data.reset();

  m_max_layer_size = 0;
}

inline void
PartialQueue::clear() {
  if (m_data == NULL)
    return;

  m_index = 0;
  m_ceiling = ceiling(num_layers - 1);

  m_layers = {};
}

inline bool
PartialQueue::insert(key_type key, mapped_type value) {
  if (key >= m_ceiling)
    return false;

  size_type idx = 0;

  // Hmm... since we already check the 'm_ceiling' above, we only need
  // to find the target layer. Could this be calculated directly?
  while (key >= ceiling(idx))
    ++idx;

  m_index = std::min(m_index, idx);

  // Currently don't allow overflow.
  if (is_layer_full(idx))
    throw std::logic_error("PartialQueue::insert(...) layer already full.");
    //return false;

  m_data.get()[m_max_layer_size * idx + m_layers[idx].second] = value;
  m_layers[idx].second++;

  if (is_layer_full(idx))
    // Set the ceiling to 0 when layer 0 is full so no more values can
    // be inserted.
    m_ceiling = idx > 0 ? ceiling(idx - 1) : 0;

  return true;
}

// is_empty() will iterate to the first layer with un-popped elements
// and return true, else return false when it reaches a overflowed or
// the last layer.
inline bool
PartialQueue::prepare_pop() {
  while (m_layers[m_index].first == m_layers[m_index].second) {
    if (is_layer_full(m_index) || m_index + 1 == num_layers)
      return false;

    m_index++;
  }

  return true;
}

inline PartialQueue::mapped_type
PartialQueue::pop() {
  if (m_index >= num_layers || m_layers[m_index].first >= m_layers[m_index].second)
    throw std::logic_error("PartialQueue::pop() bad state.");

  return m_data.get()[m_index * m_max_layer_size + m_layers[m_index].first++];
}

} // namespace torrent::utils

#endif // LIBTORRENT_UTILS_PARTIAL_QUEUE_H

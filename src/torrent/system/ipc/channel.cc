#include "config.h"

#include "torrent/system/ipc/channel.h"

#include <algorithm>
#include <cstring>
#include <new>

#include "torrent/exceptions.h"

namespace torrent::system::ipc {

namespace {

constexpr uint32_t
align_size_to_cacheline(uint32_t size) {
  uint32_t cache_line_size = LT_SMP_CACHE_BYTES;

  return (size + (cache_line_size - 1)) & ~(cache_line_size - 1);
}

inline char* as_c_ptr(Channel::header_type* header) { return reinterpret_cast<char*>(header); }

} // namespace anonymous

inline Channel::header_type*
Channel::header_at_offset(uint32_t offset) {
  return reinterpret_cast<header_type*>(static_cast<char*>(m_addr) + offset);
}

void
Channel::initialize(void* addr, size_t size) {
  if (size == 0 || (size % std::hardware_destructive_interference_size) != 0)
    throw torrent::internal_error("Channel::initialize() size must be non-zero and a multiple of cache line size");

  m_addr = static_cast<char*>(addr) + align_size_to_cacheline(sizeof(Channel));
  m_size = size - align_size_to_cacheline(sizeof(Channel));

  // TODO: This was supposed to be a 10% buffer of the channel size to be used for closing channel
  // connections.
  m_write_threshold = align_size_to_cacheline(m_size / 10) + 2 * cache_line_size;

  m_read_offset    = 0;
  m_write_offset   = 0;
  m_consumer_state = 0;
}

// TODO: Make this return both sides of the wrap, so we can do 2x writes?

uint32_t
Channel::available_write() {
  uint32_t end_offset   = m_write_offset.load(std::memory_order_acquire);
  uint32_t start_offset = m_read_offset.load(std::memory_order_acquire);

  if (end_offset >= start_offset)
    return std::max(m_size - end_offset, start_offset);

  return start_offset - end_offset;
}

bool
Channel::can_write(uint32_t size) {
  return available_write() >= align_size_to_cacheline(header_size + size) + cache_line_size;
}

// TODO: Need to align writes?

// TODO: When returning false, we need to also save the start_offset to indicate when we need to wake up producer.
// TODO: To handle the cases when a spurious write happens later, clear out the wakeup_offset.
// TODO: We set the wakeup_offset, then compare again to the start_offset, if changed we call write again.

// TODO: Reconsider above, we should never fail on write() if we check available_write() first.
// TODO: Perhaps add a strict available_write_or_wakeup function that strictly handles this.

bool
Channel::write(uint32_t id, uint32_t size, void* data) {
  if (id == 0)
    throw torrent::internal_error("Channel::write() invalid id");

  if (size > m_size - header_size)
    throw torrent::internal_error("Channel::write() invalid size");

  size_t total_size = align_size_to_cacheline(header_size + size);

  // If we're wrapping around, add a padding header. (size == 0)
  size_t start_offset = m_read_offset.load(std::memory_order_acquire);
  size_t end_offset   = m_write_offset.load(std::memory_order_acquire);

  // We keep a cache line free to distinguish full/empty.

  if (end_offset < start_offset) {
    // We're in wrapped state.
    if (start_offset - end_offset < total_size + cache_line_size)
      return false;

  } else if (end_offset == m_size) {
    // At end, need to wrap.
    if (start_offset < total_size + cache_line_size)
      return false;

    end_offset = 0;

  } else if (m_size - end_offset < total_size) {
    // Not enough space at end, need to wrap.
    if (start_offset < total_size + cache_line_size)
      return false;

    auto padding_header = header_at_offset(end_offset);
    padding_header->size = ~uint32_t{};
    padding_header->id   = 0;

    end_offset = 0;

  } else {
    // Sufficient space at end.
  }

  auto header = header_at_offset(end_offset);
  header->size = size;
  header->id   = id;

  size_t new_end_offset = end_offset + total_size;

  if (new_end_offset % cache_line_size != 0)
    throw torrent::internal_error("Channel::write() new_end_offset not aligned to cache line size");

  if (new_end_offset > m_size)
    throw torrent::internal_error("Channel::write() new_end_offset exceeds buffer size");

  if (new_end_offset == m_size)
    new_end_offset = 0;

  std::memcpy(header->data, data, size);

  m_write_offset.store(new_end_offset, std::memory_order_release);

  std::atomic_thread_fence(std::memory_order_release);
  return true;
}

Channel::header_type*
Channel::read_header() {
  size_t start_offset = m_read_offset.load(std::memory_order_acquire);
  size_t end_offset   = m_write_offset.load(std::memory_order_acquire);

  if (start_offset == end_offset)
    return nullptr;

  std::atomic_thread_fence(std::memory_order_acquire);

  auto header = header_at_offset(start_offset);

  if (header->size == ~uint32_t{0}) {
    // Padding header, wrap around.
    if (start_offset < end_offset)
      throw torrent::internal_error("Channel::read_header() padding header but no wrap");

    start_offset = 0;
    header       = header_at_offset(start_offset);

    if (start_offset == end_offset)
      throw torrent::internal_error("Channel::read_header() padding header but no data after wrap");

    if (header->size == ~uint32_t{0})
      throw torrent::internal_error("Channel::read_header() consecutive padding headers");
  }

  if (header->data + header->size > as_c_ptr(header_at_offset(m_size)))
    throw torrent::internal_error("Channel::read_header() header size exceeds buffer size");

  return header;
}

void
Channel::consume_header(header_type* header) {
  size_t header_offset    = as_c_ptr(header) - as_c_ptr(header_at_offset(0));
  size_t new_start_offset = header_offset + align_size_to_cacheline(header_size + header->size);

  if (new_start_offset > m_size)
    throw torrent::internal_error("Channel::consume_header() new_start_offset exceeds buffer size");

  if (new_start_offset == m_size)
    new_start_offset = 0;

  m_read_offset.store(new_start_offset, std::memory_order_release);
}

} // namespace torrent::system::ipc

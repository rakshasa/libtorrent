#include "config.h"

#include "torrent/shm/channel.h"

#include <algorithm>
#include <cstring>
#include <new>

#include "torrent/exceptions.h"

namespace torrent::shm {

constexpr uint32_t
align_to_cacheline(uint32_t size) {
  uint32_t cache_line_size = std::hardware_destructive_interference_size;
  return (size + (cache_line_size - 1)) & ~(cache_line_size - 1);
}

void
Channel::initialize(void* addr, size_t size) {
  if (size == 0 || (size % std::hardware_destructive_interference_size) != 0)
    throw torrent::internal_error("Channel::initialize() size must be non-zero and a multiple of cache line size");

  m_addr = static_cast<char*>(addr) + align_to_cacheline(sizeof(Channel));
  m_size = size - align_to_cacheline(sizeof(Channel));

  m_read_offset = 0;
  m_write_offset = 0;
}

uint32_t
Channel::available_write() {
  // These are cacheline aligned atomic values.
  uint32_t start_offset = m_read_offset.load(std::memory_order_acquire);
  uint32_t end_offset   = m_write_offset.load(std::memory_order_acquire);

  if (end_offset >= start_offset)
    return std::max(m_size - end_offset, start_offset);
  else
    return start_offset - end_offset;
}

// TODO: Need to align writes?

bool
Channel::write(uint32_t id, uint32_t size, void* data) {
  if (id == 0)
    throw torrent::internal_error("Channel::write() invalid id");

  if (size > m_size - header_size)
    throw torrent::internal_error("Channel::write() invalid size");

  size_t total_size = align_to_cacheline(header_size + size);

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

    auto padding_header = reinterpret_cast<header_type*>(static_cast<char*>(m_addr) + end_offset);
    padding_header->size = ~uint32_t{0};
    padding_header->id   = 0;

    end_offset = 0;

  } else {
    // Sufficient space at end.
  }

  auto header = reinterpret_cast<header_type*>(static_cast<char*>(m_addr) + end_offset);
  header->size = size;
  header->id   = id;

  size_t new_end_offset = end_offset + total_size;

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

  auto header = reinterpret_cast<header_type*>(static_cast<char*>(m_addr) + start_offset);

  if (header->size == ~uint32_t{0}) {
    // Padding header, wrap around.
    if (start_offset < end_offset)
      throw torrent::internal_error("Channel::read_header() padding header but no wrap");

    start_offset = 0;
    header = reinterpret_cast<header_type*>(static_cast<char*>(m_addr) + start_offset);

    if (start_offset == end_offset)
      throw torrent::internal_error("Channel::read_header() padding header but no data after wrap");

    if (header->size == ~uint32_t{0})
      throw torrent::internal_error("Channel::read_header() consecutive padding headers");
  }

  if (header->data + header->size > static_cast<char*>(m_addr) + m_size)
    throw torrent::internal_error("Channel::read_header() header size exceeds buffer size");

  return header;
}

void
Channel::consume_header(header_type* header) {
  size_t header_offset    = reinterpret_cast<char*>(header) - static_cast<char*>(m_addr);
  size_t new_start_offset = header_offset + align_to_cacheline(header_size + header->size);

  if (new_start_offset > m_size)
    throw torrent::internal_error("Channel::consume_header() new_start_offset exceeds buffer size");

  if (new_start_offset == m_size)
    new_start_offset = 0;

  m_read_offset.store(new_start_offset, std::memory_order_release);
}

} // namespace torrent::shm

#ifndef LIBTORRENT_STORAGE_CHUNK_H
#define LIBTORRENT_STORAGE_CHUNK_H

#include <algorithm>
#include <functional>
#include <vector>

#include "chunk_part.h"

namespace torrent {

class lt_cacheline_aligned Chunk : private std::vector<ChunkPart> {
public:
  typedef std::vector<ChunkPart>    base_type;
  typedef std::pair<void*,uint32_t> data_type;

  using base_type::value_type;

  using base_type::iterator;
  using base_type::reverse_iterator;
  using base_type::const_iterator;
  using base_type::empty;
  using base_type::reserve;

  using base_type::begin;
  using base_type::end;
  using base_type::rbegin;
  using base_type::rend;

  using base_type::front;
  using base_type::back;

  Chunk() : m_chunkSize(0), m_prot(~0) {}
  ~Chunk() { clear(); }

  bool                is_all_valid() const;

  // All permissions are set for empty chunks.
  bool                is_readable() const             { return m_prot & MemoryChunk::prot_read; }
  bool                is_writable() const             { return m_prot & MemoryChunk::prot_write; }
  bool                has_permissions(int prot) const { return !(prot & ~m_prot); }

  uint32_t            chunk_size() const              { return m_chunkSize; }

  void                clear();

  void                push_back(value_type::mapped_type mapped, const MemoryChunk& c);

  // The at_position functions only returns non-zero length iterators
  // or end.
  iterator            at_position(uint32_t pos);
  iterator            at_position(uint32_t pos, iterator itr);

  data_type           at_memory(uint32_t offset, iterator part);

  iterator            find_address(void* ptr);

  // Check how much of the chunk is incore from pos.
  bool                is_incore(uint32_t pos, uint32_t length = ~uint32_t());
  uint32_t            incore_length(uint32_t pos, uint32_t length = ~uint32_t());

  bool                sync(int flags);

  void                preload(uint32_t position, uint32_t length, bool useAdvise);

  bool                to_buffer(void* buffer, uint32_t position, uint32_t length);
  bool                from_buffer(const void* buffer, uint32_t position, uint32_t length);
  bool                compare_buffer(const void* buffer, uint32_t position, uint32_t length);

private:
  Chunk(const Chunk&);
  void operator = (const Chunk&);

  uint32_t            m_chunkSize;
  int                 m_prot;
};

inline Chunk::iterator
Chunk::at_position(uint32_t pos, iterator itr) {
  while (itr != end() && itr->position() + itr->size() <= pos)
    itr++;

  return itr;
}

inline Chunk::iterator
Chunk::find_address(void* ptr) {
  return std::find_if(begin(), end(), [&](auto& part) { return part.has_address(ptr); });
}

}

#endif

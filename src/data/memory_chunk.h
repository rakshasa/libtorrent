#ifndef LIBTORRENT_DATA_MEMORY_CHUNK_H
#define LIBTORRENT_DATA_MEMORY_CHUNK_H

#include <algorithm>
#include <cinttypes>
#include <sys/mman.h>
#include <cstddef>
#include <memory>

namespace torrent {

class MemoryChunk {
 public:
  // Consider information about whetever the memory maps to a file or
  // not, since mincore etc can only be called on files.

  static constexpr int prot_exec              = PROT_EXEC;
  static constexpr int prot_read              = PROT_READ;
  static constexpr int prot_write             = PROT_WRITE;
  static constexpr int prot_none              = PROT_NONE;
  static constexpr int map_shared             = MAP_SHARED;
  static constexpr int map_anon               = MAP_ANON;

#ifdef USE_MADVISE
  static constexpr int advice_normal          = MADV_NORMAL;
  static constexpr int advice_random          = MADV_RANDOM;
  static constexpr int advice_sequential      = MADV_SEQUENTIAL;
  static constexpr int advice_willneed        = MADV_WILLNEED;
  static constexpr int advice_dontneed        = MADV_DONTNEED;
#else
  static constexpr int advice_normal          = 0;
  static constexpr int advice_random          = 1;
  static constexpr int advice_sequential      = 2;
  static constexpr int advice_willneed        = 3;
  static constexpr int advice_dontneed        = 4;
#endif
  static constexpr int sync_sync              = MS_SYNC;
  static constexpr int sync_async             = MS_ASYNC;
  static constexpr int sync_invalidate        = MS_INVALIDATE;

  MemoryChunk() = default;
  ~MemoryChunk() { clear(); }
  MemoryChunk(const MemoryChunk&) = default;
  MemoryChunk& operator=(const MemoryChunk&) = default;

  // Doesn't allow ptr == NULL, use the default ctor instead.
  MemoryChunk(char* ptr, char* begin, char* end, int prot, int flags);

  bool                is_valid() const                                     { return m_ptr; }
  bool                is_readable() const                                  { return m_prot & PROT_READ; }
  bool                is_writable() const                                  { return m_prot & PROT_WRITE; }
  bool                is_exec() const                                      { return m_prot & PROT_EXEC; }

  inline bool         is_valid_range(uint32_t offset, uint32_t length) const;

  bool                has_permissions(int prot) const                      { return !(prot & ~m_prot); }

  char*               ptr() const                                          { return m_ptr; }
  char*               begin() const                                        { return m_begin; }
  char*               end() const                                          { return m_end; }

  int                 get_prot() const                                     { return m_prot; }

  uint32_t            size() const                                         { return m_end - m_begin; }
  uint32_t            size_aligned() const;
  inline void         clear();
  void                unmap();

  // Use errno and strerror if you want to know why these failed.
  void                incore(char* buf, uint32_t offset, uint32_t length);
  bool                advise(uint32_t offset, uint32_t length, int advice);
  bool                sync(uint32_t offset, uint32_t length, int flags);

  bool                is_incore(uint32_t offset, uint32_t length);

  // Helper functions for aligning offsets and ranges to page boundaries.

  uint32_t            page_align() const                                   { return m_begin - m_ptr; }
  uint32_t            page_align(uint32_t o) const                         { return (o + page_align()) % m_pagesize; }

  // This won't return correct values if length == 0.
  inline uint32_t     pages_touched(uint32_t offset, uint32_t length) const;

  static uint32_t     page_size()                                          { return m_pagesize; }

private:
  inline void         align_pair(uint32_t* offset, uint32_t* length) const;

  static uint32_t     m_pagesize;

  char*               m_ptr{};
  char*               m_begin{};
  char*               m_end{};

  int                 m_prot;
  int                 m_flags{PROT_NONE};
};

inline bool
MemoryChunk::is_valid_range(uint32_t offset, uint32_t length) const {
  return length != 0 && static_cast<uint64_t>(offset) + length <= size();
}

inline void
MemoryChunk::clear() {
  m_ptr = m_begin = m_end = NULL; m_flags = PROT_NONE;
}

inline uint32_t
MemoryChunk::pages_touched(uint32_t offset, uint32_t length) const {
  if (length == 0)
    return 0;

  return (length + page_align(offset) + m_pagesize - 1) / m_pagesize;
}

// The size of the mapped memory.
inline uint32_t
MemoryChunk::size_aligned() const {
  return std::distance(m_ptr, m_end) + page_size() - ((std::distance(m_ptr, m_end) - 1) % page_size() + 1);
}

inline bool
MemoryChunk::is_incore(uint32_t offset, uint32_t length) {
  const uint32_t size = pages_touched(offset, length);
  auto buf = std::make_unique<char[]>(size);
  auto begin = buf.get();
  auto end = begin + size;

  incore(begin, offset, length);

  return std::find(begin, end, 0) == end;
}

} // namespace torrent

#endif

#ifndef LIBTORRENT_TORRENT_SYSTEM_IPC_SEGMENT_H
#define LIBTORRENT_TORRENT_SYSTEM_IPC_SEGMENT_H

#include <torrent/common.h>

namespace torrent::system::ipc {

class LIBTORRENT_EXPORT Segment {
public:
  static constexpr size_t page_size = 4096;

  Segment() = default;
  ~Segment();

  void                create(uint32_t size);
  void                destroy();

  void*               address();
  size_t              size() const;

private:
  size_t              m_size{};
  void*               m_addr{};
};

inline Segment::~Segment()          { m_size = 0; m_addr = nullptr; }

inline void*  Segment::address()    { return m_addr; }
inline size_t Segment::size() const { return m_size; }

}

#endif

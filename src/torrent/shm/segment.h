#ifndef LIBTORRENT_TORRENT_SHM_SEGMENT_H
#define LIBTORRENT_TORRENT_SHM_SEGMENT_H

#include <torrent/common.h>

// Check limits with: sysctl -A | grep shm

namespace torrent::shm {

class LIBTORRENT_EXPORT Segment {
public:
  static constexpr size_t page_size = 4096;

  Segment() = default;
  ~Segment();

  void                create(uint32_t size);

  void*               address()    { return m_addr; }
  size_t              size() const { return m_size; }

private:
  void                unlink();

  void                attach();
  void                detach();

  int                 m_shm_id{-1};
  size_t              m_size{};
  void*               m_addr{};
};

inline
Segment::~Segment() {
  m_shm_id = -1;
  m_size = 0;
  m_addr = nullptr;
}

}

#endif // LIBTORRENT_TORRENT_SHM_SEGMENT_H

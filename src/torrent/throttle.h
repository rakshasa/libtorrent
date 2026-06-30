#ifndef LIBTORRENT_TORRENT_THROTTLE_H
#define LIBTORRENT_TORRENT_THROTTLE_H

#include <torrent/common.h>

namespace RTORRENT_EXPORT torrent {

class ThrottleList;
class ThrottleInternal;

class Throttle {
public:
  static Throttle*    create_throttle();
  static void         destroy_throttle(Throttle* throttle);

  Throttle*           create_slave();

  bool                is_throttled() const;

  // 0 == UNLIMITED.
  uint64_t            max_rate() const { return m_maxRate; }
  void                set_max_rate(uint64_t v);

  const Rate*         rate() const;

  ThrottleList*       throttle_list()  { return m_throttleList; }

protected:
  Throttle()  = default;
  ~Throttle() = default;

  ThrottleInternal*       m_ptr()       { return reinterpret_cast<ThrottleInternal*>(this); }
  const ThrottleInternal* c_ptr() const { return reinterpret_cast<const ThrottleInternal*>(this); }

  uint32_t            calculate_min_chunk_size() const;
  uint32_t            calculate_max_chunk_size() const;
  uint32_t            calculate_interval() const;

  uint64_t            m_maxRate;

  ThrottleList*       m_throttleList;
};

} // namespace torrent

#endif

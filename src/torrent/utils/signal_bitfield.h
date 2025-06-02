// Allows a thread to define work it will do when receiving a signal.

#ifndef LIBTORRENT_UTILS_SIGNAL_BITFIELD_H
#define LIBTORRENT_UTILS_SIGNAL_BITFIELD_H

#include <atomic>
#include <functional>
#include <thread>

#include <torrent/common.h>

namespace torrent {

class LIBTORRENT_EXPORT signal_bitfield {
public:
  using bitfield_type = uint32_t;
  using slot_type     = std::function<void()>;

  static constexpr unsigned int max_size = 32;

  signal_bitfield() = default;

  unsigned int  add_signal(const slot_type& slot);
  bool          has_signal(unsigned int index) const { return m_bitfield & (1 << index); }

  void          signal(unsigned int index) { m_bitfield |= 1 << index; }
  void          work();

  void          handover(std::thread::id thread_id) { m_thread_id = thread_id; }

private:
  unsigned int                 m_size{0};
  slot_type                    m_slots[max_size];

  std::atomic<std::thread::id> m_thread_id{std::this_thread::get_id()};
  std::atomic<bitfield_type>   m_bitfield{0};
};

} // namespace torrent

#endif

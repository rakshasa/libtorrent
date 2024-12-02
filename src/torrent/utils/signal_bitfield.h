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
  typedef uint32_t               bitfield_type;
  typedef std::function<void ()> slot_type;

  static const unsigned int max_size = 32;

  signal_bitfield() : m_thread_id(std::this_thread::get_id()), m_size(0), m_bitfield(0) {}

  unsigned int  add_signal(slot_type slot);
  bool          has_signal(unsigned int index) const { return m_bitfield & (1 << index); }

  void          signal(unsigned int index) { m_bitfield |= 1 << index; }
  void          work();

  void          handover(std::thread::id thread_id) { m_thread_id = thread_id; }

private:
  std::thread::id m_thread_id;
  unsigned int    m_size;
  slot_type       m_slots[max_size];

  std::atomic<bitfield_type> m_bitfield;
};

}

#endif

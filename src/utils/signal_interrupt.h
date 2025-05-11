#ifndef LIBTORRENT_UTILS_SIGNAL_INTERRUPT_H
#define LIBTORRENT_UTILS_SIGNAL_INTERRUPT_H

#include <atomic>
#include <memory>
#include <utility>
#include <torrent/event.h>

namespace torrent {

class LIBTORRENT_EXPORT SignalInterrupt : public Event {
public:
  using pair_type = std::pair<std::unique_ptr<SignalInterrupt>, std::unique_ptr<SignalInterrupt>>;

  ~SignalInterrupt();

  static pair_type    create_pair();

  bool                is_poking() const { return m_poking.load(); }

  void                poke();

  void                event_read();
  void                event_write();
  void                event_error();

private:
  SignalInterrupt(const SignalInterrupt&) = delete;
  SignalInterrupt& operator=(const SignalInterrupt&) = delete;
  SignalInterrupt(int fd);

  SignalInterrupt*    m_other;
  std::atomic_bool    m_poking{false};
};

}

#endif // LIBTORRENT_UTILS_SIGNAL_INTERRUPT_H

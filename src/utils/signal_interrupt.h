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

  ~SignalInterrupt() override;

  static pair_type    create_pair();

  bool                is_poking() const { return m_poking.load(); }

  void                poke();

  const char*         type_name() const override;

  void                event_read() override;
  void                event_write() override;
  void                event_error() override;

private:
  SignalInterrupt(const SignalInterrupt&) = delete;
  SignalInterrupt& operator=(const SignalInterrupt&) = delete;
  SignalInterrupt(int fd);

  SignalInterrupt*    m_other;
  std::atomic_bool    m_poking{false};
};

} // namespace torrent

#endif // LIBTORRENT_UTILS_SIGNAL_INTERRUPT_H

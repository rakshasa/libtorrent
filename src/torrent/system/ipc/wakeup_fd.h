#ifndef LIBTORRENT_TORRENT_SYSTEM_IPC_WAKEUP_FD_H
#define LIBTORRENT_TORRENT_SYSTEM_IPC_WAKEUP_FD_H

#include <atomic>
#include <torrent/system/event.h>

namespace torrent::system::ipc {

// Cross-process wakeup, using eventfd for linux and FreeBSD 13+, and socketpair for other platforms.
//
// The caller uses a shared memory segment with an atomic flag to signal that a wakeup is needed.

class LIBTORRENT_EXPORT WakeupFd : public Event {
public:
  WakeupFd() = default;

  static std::pair<int, int> create_fd_pair();

  const char*         type_name() const override { return "wakeup_fd"; }

  void                open(std::pair<int, int> fd_pair, bool is_parent);
  void                close();

  void                send_interrupt();

private:
  void                event_read() override;
  void                event_write() override;
  void                event_error() override;
};

} // namespace torrent::system::ipc

#endif

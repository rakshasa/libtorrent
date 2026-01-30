#ifndef LIBTORRENT_TORRENT_RUNTIME_SOCKET_MANAGER_H
#define LIBTORRENT_TORRENT_RUNTIME_SOCKET_MANAGER_H

#include <mutex>
#include <unordered_map>
#include <torrent/common.h>

// A socket that is closed by the kernel while neitehr ready nor write polling will be considered
// uninterested by the kernel, and possibly reused.
//
// Those sockets must be marked as 'inactive' in the socket manager, and when entering active state
// it must check the socket manager if the socket is still valid.
//
// File and other socket types that do not unexpectedly get closed by the kernel do not need to be
// managed.

namespace torrent::runtime {

struct SocketInfo {
  // TODO: Replace with Event*, and add thread to PollEvent?
  // TODO: Event should contain the owning thread, and not be included here.
  // TODO: Fd not needed here.

  int                 fd{-1};
  Event*              event{};
  utils::Thread*      thread{};
  int                 flags{};
};

class LIBTORRENT_EXPORT SocketManager {
public:
  static constexpr int flag_inactive = (1 << 0);

  SocketManager();
  ~SocketManager();

  // TODO: Rename / change _or_throw to be more specific about what we throw, as it currently calls
  // internal errors. DHT server should probably throw resource_error instead? Or retry. (this
  // however hides errors for normal users)
  //
  // TODO: Finish implementation of reuse logic.

  // To avoid reuse race conditions, these calls must also add the Event to read/write polling.

  // Throw internal_error on conflicts, as the caller isn't expecting conflicts.
  void                open_event_or_throw(Event* event, std::function<void ()> func);

  // Try to reuse existing socket, if that fails call cleanup.
  //
  // This is used for listen accept and other cases where the caller doesn't mind ignoring failures.
  bool                open_event_or_cleanup(Event* event, std::function<void ()> func, std::function<void ()> cleanup);

  void                close_event_or_throw(Event* event, std::function<void ()> func);

  // Event already opened the socket, just register it.
  void                register_event_or_throw(Event* event, std::function<void ()> func);
  void                unregister_event_or_throw(Event* event, std::function<void ()> func);

  // The func must close event_from and the pointer must remain valid.
  Event*              transfer_event(Event* event_from, std::function<Event* ()> func);

  bool                execute_if_not_present(int fd, std::function<void ()> func);

  void                mark_event_active(Event* event);
  void                mark_event_inactive(Event* event);

  // No, this should take Event*?
  // bool                is_socket_reused(int fd);

protected:

  auto                lock_guard() { return std::lock_guard(m_mutex); }

private:
  using socket_map = std::unordered_map<int, SocketInfo>;

  bool                handle_reused_socket(socket_map::iterator itr);

  std::mutex          m_mutex;

  socket_map          m_socket_map;
};

} // namespace torrent::runtime

#endif // LIBTORRENT_TORRENT_RUNTIME_SOCKET_MANAGER_H

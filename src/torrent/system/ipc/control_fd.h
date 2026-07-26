#ifndef LIBTORRENT_TORRENT_SYSTEM_IPC_CONTROL_FD_H
#define LIBTORRENT_TORRENT_SYSTEM_IPC_CONTROL_FD_H

#include <atomic>
#include <torrent/system/event.h>

namespace torrent::system::ipc {

class ControlFd;

class LIBTORRENT_EXPORT PublicControlFd {
public:
  PublicControlFd(ControlFd* control_fd) : m_control_fd(control_fd) {}

  void register_interrupt_handler(std::function<void()>&& fn);
  void register_message_handler(std::function<void(std::string)>&& fn);

  void register_closed_handler(std::function<void(int)>&& fn);
  void register_shutdown_handler(std::function<void(bool)>&& fn);

private:
  ControlFd* m_control_fd;
};

class LIBTORRENT_EXPORT ControlFd : public Event {
public:
  static constexpr unsigned int max_message_size = 1024;

  ControlFd() = default;
  ~ControlFd() = default;

  const char*         type_name() const override { return "ipc-channel"; }

  void                open(int fd);
  void                close();

  void                send_interrupt();

  void                send_graceful_shutdown();
  void                send_forceful_shutdown();
  void                send_fatal_error(const char* msg, uint32_t size);

private:
  friend class PublicControlFd;

  void                send_shutdown_message(bool graceful);
  void                send_message_internal(const char* msg, uint32_t size);

  void                event_read() override;
  void                event_write() override;
  void                event_error() override;

  std::function<void()>            m_slot_interrupt;
  std::function<void(std::string)> m_slot_message;
  std::function<void(int)>         m_slot_closed;
  std::function<void(bool)>        m_slot_shutdown;
};

inline void PublicControlFd::register_interrupt_handler(std::function<void()>&& fn)          { m_control_fd->m_slot_interrupt = std::move(fn); }
inline void PublicControlFd::register_message_handler(std::function<void(std::string)>&& fn) { m_control_fd->m_slot_message   = std::move(fn); }
inline void PublicControlFd::register_closed_handler(std::function<void(int)>&& fn)          { m_control_fd->m_slot_closed    = std::move(fn); }
inline void PublicControlFd::register_shutdown_handler(std::function<void(bool)>&& fn)       { m_control_fd->m_slot_shutdown  = std::move(fn); }

inline void ControlFd::send_graceful_shutdown() { send_shutdown_message(true); }
inline void ControlFd::send_forceful_shutdown() { send_shutdown_message(false); }

} // namespace torrent::system::ipc

#endif

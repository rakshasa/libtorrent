#ifndef LIBTORRENT_TORRENT_SYSTEM_POLL_H
#define LIBTORRENT_TORRENT_SYSTEM_POLL_H

#include <memory>
#include <vector>
#include <torrent/system/common.h>

namespace torrent::system {

class PollEvent;
class PollInternal;

class LIBTORRENT_EXPORT Poll {
public:
  static std::unique_ptr<Poll> create();

  ~Poll();

  // TODO: Make protected.
  unsigned int        do_poll(std::chrono::microseconds timeout);
  void                do_interrupt();

  // The open max value is used when initializing libtorrent, it
  // should be less than or equal to sysconf(_SC_OPEN_MAX).
  uint32_t            open_max() const;

  // Event::get_fd() is guaranteed to be valid and remain constant
  // from open(...) is called to close(...) returns. The implementor
  // of this class should not open nor close the file descriptor.
  void                open(system::Event* event);
  void                close(system::Event* event);

  // Functions for checking whetever the Event is listening to r/w/e?
  bool                in_read(system::Event* event);
  bool                in_write(system::Event* event);

  // These functions may be called on 'event's that might, or might
  // not, already be in the set.
  void                insert_read(system::Event* event);
  void                insert_write(system::Event* event);

  void                remove_read(system::Event* event);
  void                remove_write(system::Event* event);

  void                remove_and_close(system::Event* event);

  // Add one for HUP? Or would that be in event?

protected:
  friend class Thread;

  void                init_thread();
  void                cleanup_thread();

private:
  using poll_event_list = std::vector<std::shared_ptr<PollEvent>>;

  static constexpr int flag_polling     = 0x1;
  static constexpr int flag_interrupted = 0x2;
  static constexpr int flag_state_mask  = 0x3;

  Poll() = default;
  Poll(const Poll&) = delete;
  Poll& operator=(const Poll&) = delete;

  int                 poll(std::chrono::microseconds timeout);
  unsigned int        process();

  bool                m_processing{false};
  poll_event_list     m_closed_events;

  std::unique_ptr<PollInternal> m_internal;

  align_cacheline std::atomic<int> m_polling_state{};
};

} // namespace torrent

#endif

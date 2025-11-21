#ifndef LIBTORRENT_TORRENT_POLL_H
#define LIBTORRENT_TORRENT_POLL_H

#include <memory>
#include <torrent/common.h>

namespace torrent::net {

class PollInternal;

class LIBTORRENT_EXPORT Poll {
public:
  static std::unique_ptr<Poll> create();

  ~Poll();

  // TODO: Make protected.
  unsigned int        do_poll(int64_t timeout_usec);

  // The open max value is used when initializing libtorrent, it
  // should be less than or equal to sysconf(_SC_OPEN_MAX).
  uint32_t            open_max() const;

  // Event::get_fd() is guaranteed to be valid and remain constant
  // from open(...) is called to close(...) returns. The implementor
  // of this class should not open nor close the file descriptor.
  void                open(Event* event);
  void                close(Event* event);

  // Functions for checking whetever the Event is listening to r/w/e?
  bool                in_read(Event* event);
  bool                in_write(Event* event);
  bool                in_error(Event* event);

  // These functions may be called on 'event's that might, or might
  // not, already be in the set.
  void                insert_read(Event* event);
  void                insert_write(Event* event);
  void                insert_error(Event* event);

  void                remove_read(Event* event);
  void                remove_write(Event* event);
  void                remove_error(Event* event);

  void                remove_and_close(Event* event);

  // Add one for HUP? Or would that be in event?

private:
  Poll() = default;
  Poll(const Poll&) = delete;
  Poll& operator=(const Poll&) = delete;

  int                 poll(int timeout_usec);
  unsigned int        process();

  std::unique_ptr<PollInternal> m_internal;
};

} // namespace torrent

#endif

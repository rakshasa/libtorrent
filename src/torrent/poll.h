#ifndef LIBTORRENT_TORRENT_POLL_H
#define LIBTORRENT_TORRENT_POLL_H

#include <functional>
#include <torrent/common.h>

namespace torrent {

class Event;

class LIBTORRENT_EXPORT Poll {
public:
  using slot_poll = std::function<Poll*()>;

  static constexpr int      poll_worker_thread     = 0x1;
  static constexpr uint32_t flag_waive_global_lock = 0x1;

  virtual ~Poll() = default;

  uint32_t            flags() const { return m_flags; }
  void                set_flags(uint32_t flags) { m_flags = flags; }

  virtual unsigned int do_poll(int64_t timeout_usec, int flags = 0) = 0;

  // The open max value is used when initializing libtorrent, it
  // should be less than or equal to sysconf(_SC_OPEN_MAX).
  virtual uint32_t    open_max() const = 0;

  // Event::get_fd() is guaranteed to be valid and remain constant
  // from open(...) is called to close(...) returns. The implementor
  // of this class should not open nor close the file descriptor.
  virtual void        open(Event* event) = 0;
  virtual void        close(Event* event) = 0;

  // More efficient interface when closing the file descriptor.
  // Automatically removes the event from all polls.
  // Event::get_fd() may or may not be closed already.
  virtual void        closed(Event* event) = 0;

  // Functions for checking whetever the Event is listening to r/w/e?
  virtual bool        in_read(Event* event) = 0;
  virtual bool        in_write(Event* event) = 0;
  virtual bool        in_error(Event* event) = 0;

  // These functions may be called on 'event's that might, or might
  // not, already be in the set.
  virtual void        insert_read(Event* event) = 0;
  virtual void        insert_write(Event* event) = 0;
  virtual void        insert_error(Event* event) = 0;

  virtual void        remove_read(Event* event) = 0;
  virtual void        remove_write(Event* event) = 0;
  virtual void        remove_error(Event* event) = 0;

  // Add one for HUP? Or would that be in event?

  static slot_poll&   slot_create_poll() { return m_slot_create_poll; }

private:
  static slot_poll    m_slot_create_poll;

  uint32_t            m_flags{0};
};

}

#endif

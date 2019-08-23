// libTorrent - BitTorrent library
// Copyright (C) 2005-2011, Jari Sundell
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
// In addition, as a special exception, the copyright holders give
// permission to link the code of portions of this program with the
// OpenSSL library under certain conditions as described in each
// individual source file, and distribute linked combinations
// including the two.
//
// You must obey the GNU General Public License in all respects for
// all of the code used other than OpenSSL.  If you modify file(s)
// with this exception, you may extend this exception to your version
// of the file(s), but you are not obligated to do so.  If you do not
// wish to do so, delete this exception statement from your version.
// If you delete this exception statement from all source files in the
// program, then also delete it here.
//
// Contact:  Jari Sundell <jaris@ifi.uio.no>
//
//           Skomakerveien 33
//           3185 Skoppum, NORWAY

#ifndef LIBTORRENT_TORRENT_POLL_H
#define LIBTORRENT_TORRENT_POLL_H

#include <functional>
#include <torrent/common.h>

namespace torrent {

class Event;

class LIBTORRENT_EXPORT Poll {
public:
  typedef std::function<Poll* ()> slot_poll;

  static const int      poll_worker_thread     = 0x1;
  static const uint32_t flag_waive_global_lock = 0x1;

  Poll() : m_flags(0) {}
  virtual ~Poll() {}

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

  uint32_t            m_flags;
};

}

#endif

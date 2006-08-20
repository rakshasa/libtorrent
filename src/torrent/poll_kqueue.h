// libTorrent - BitTorrent library
// Copyright (C) 2005-2006, Jari Sundell
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

#ifndef LIBTORRENT_TORRENT_POLL_KQUEUE_H
#define LIBTORRENT_TORRENT_POLL_KQUEUE_H

#include <inttypes.h>
#include <vector>
#include <torrent/poll.h>

struct kevent;

namespace torrent {

class PollKQueue : public torrent::Poll {
public:
  typedef std::vector<uint32_t> Table;

  static const uint32_t flag_read  = (1 << 0);
  static const uint32_t flag_write = (1 << 1);
  static const uint32_t flag_error = (1 << 2);

  static PollKQueue*   create(int maxOpenSockets);
  virtual ~PollKQueue();

  int                 poll(int msec);
  void                perform();

  int                 file_descriptor() { return m_fd; }

  virtual uint32_t    open_max() const;

  // torrent::Event::get_fd() is guaranteed to be valid and remain constant
  // from open(...) is called to close(...) returns.
  virtual void        open(torrent::Event* event);
  virtual void        close(torrent::Event* event);

  // Functions for checking whetever the torrent::Event is listening to r/w/e?
  virtual bool        in_read(torrent::Event* event);
  virtual bool        in_write(torrent::Event* event);
  virtual bool        in_error(torrent::Event* event);

  // These functions may be called on 'event's that might, or might
  // not, already be in the set.
  virtual void        insert_read(torrent::Event* event);
  virtual void        insert_write(torrent::Event* event);
  virtual void        insert_error(torrent::Event* event);

  virtual void        remove_read(torrent::Event* event);
  virtual void        remove_write(torrent::Event* event);
  virtual void        remove_error(torrent::Event* event);

private:
  PollKQueue(int fd, int maxEvents, int maxOpenSockets);

  inline uint32_t     event_mask(Event* e);
  inline void         set_event_mask(Event* e, uint32_t m);

  void                modify(torrent::Event* event, unsigned short op, short mask);

  int                 m_fd;

  int                 m_maxEvents;
  int                 m_waitingEvents;
  int                 m_changedEvents;

  Table               m_table;
  struct kevent*      m_events;
  struct kevent*      m_changes;
};

}

#endif

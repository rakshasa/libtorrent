// libTorrent - BitTorrent library
// Copyright (C) 2005, Jari Sundell
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

namespace torrent {

// Do we use normal functions implemented by the client, class without
// virtual functinons or a base class?
//
// The client would in any case need the epoll fd, or a list of fd's
// to add to the fd_sets. So a class it is.
//
// Virtual or not? Can't really be non-virtual, as that precludes the
// client implementing additional helper member functions.

class Event;

class Poll {
public:
  virtual ~Poll() {}

  // Event::get_fd() is guaranteed to be valid and remain constant
  // from open(...) is called to close(...) returns.
  virtual void        open(Event* event) = 0;
  virtual void        close(Event* event) = 0;

  // Functions for checking whetever the Event is listening to r/w/e?
  virtual void        in_read(Event* event) = 0;
  virtual void        in_write(Event* event) = 0;
  virtual void        in_error(Event* event) = 0;

  // insert_*/erase_* will never be called on an already
  // inserted/erased file descriptor? Or do we allow this to simplify
  // the user code and avoid an extra virtual call just to check?
  virtual void        insert_read(Event* event) = 0;
  virtual void        insert_write(Event* event) = 0;
  virtual void        insert_error(Event* event) = 0;

  virtual void        remove_read(Event* event) = 0;
  virtual void        remove_write(Event* event) = 0;
  virtual void        remove_error(Event* event) = 0;

  // Add one for HUP? Or would that be in event?
};

}

#endif

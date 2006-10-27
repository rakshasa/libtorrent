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

#ifndef LIBTORRENT_NET_POLL_SELECT_H
#define LIBTORRENT_NET_POLL_SELECT_H

#include <sys/types.h>
#include <torrent/common.h>
#include <torrent/poll.h>

namespace torrent {

// The default Poll implementation using fd_set's.
//
// You should call torrent::perform() (or whatever the function will
// be called) immidiately before and after the call to work(...). This
// ensures we dealt with scheduled tasks and updated the cache'ed time.

class LIBTORRENT_EXPORT PollSelect : public Poll {
public:
  static PollSelect*  create(int maxOpenSockets);
  virtual ~PollSelect();

  virtual uint32_t    open_max() const;

  // Returns the largest fd marked.
  unsigned int        fdset(fd_set* readSet, fd_set* writeSet, fd_set* exceptSet);
  void                perform(fd_set* readSet, fd_set* writeSet, fd_set* exceptSet);

  virtual void        open(Event* event);
  virtual void        close(Event* event);

  virtual bool        in_read(Event* event);
  virtual bool        in_write(Event* event);
  virtual bool        in_error(Event* event);

  virtual void        insert_read(Event* event);
  virtual void        insert_write(Event* event);
  virtual void        insert_error(Event* event);

  virtual void        remove_read(Event* event);
  virtual void        remove_write(Event* event);
  virtual void        remove_error(Event* event);

private:
  PollSelect() {}
  PollSelect(const PollSelect&);
  void operator = (const PollSelect&);

  SocketSet*          m_readSet;
  SocketSet*          m_writeSet;
  SocketSet*          m_exceptSet;
};

}

#endif

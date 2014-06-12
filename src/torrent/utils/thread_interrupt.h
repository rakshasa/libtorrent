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

#ifndef LIBTORRENT_UTILS_THREAD_INTERRUPT_H
#define LIBTORRENT_UTILS_THREAD_INTERRUPT_H

#include <utility>
#include <torrent/event.h>

namespace torrent {

class SocketFd;

class LIBTORRENT_EXPORT lt_cacheline_aligned thread_interrupt : public Event {
public:
  typedef std::pair<thread_interrupt*, thread_interrupt*> pair_type;

  ~thread_interrupt();

  static pair_type    create_pair();

  bool                is_poking() const;

  bool                poke();

  void                event_read();
  void                event_write() {}
  void                event_error() {}

private:
  thread_interrupt(int fd);

  SocketFd&           get_fd() { return *reinterpret_cast<SocketFd*>(&m_fileDesc); }

  thread_interrupt*   m_other;
  bool                m_poking lt_cacheline_aligned;
};

inline bool
thread_interrupt::is_poking() const {
  return m_poking;
}

}

#endif

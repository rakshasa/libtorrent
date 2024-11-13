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

#ifndef LIBTORRENT_TORRENT_THROTTLE_H
#define LIBTORRENT_TORRENT_THROTTLE_H

#include <torrent/common.h>

namespace torrent {

class ThrottleList;
class ThrottleInternal;

class LIBTORRENT_EXPORT Throttle {
public:
  static Throttle*    create_throttle();
  static void         destroy_throttle(Throttle* throttle);

  Throttle*           create_slave();

  bool                is_throttled();

  // 0 == UNLIMITED.
  uint64_t            max_rate() const { return m_maxRate; }
  void                set_max_rate(uint64_t v);

  const Rate*         rate() const;

  ThrottleList*       throttle_list()  { return m_throttleList; }

protected:
  Throttle() {}
  ~Throttle() {}

  ThrottleInternal*       m_ptr()       { return reinterpret_cast<ThrottleInternal*>(this); }
  const ThrottleInternal* c_ptr() const { return reinterpret_cast<const ThrottleInternal*>(this); }

  uint32_t            calculate_min_chunk_size() const LIBTORRENT_NO_EXPORT;
  uint32_t            calculate_max_chunk_size() const LIBTORRENT_NO_EXPORT;
  uint32_t            calculate_interval() const LIBTORRENT_NO_EXPORT;

  uint64_t            m_maxRate;

  ThrottleList*       m_throttleList;
};

}

#endif

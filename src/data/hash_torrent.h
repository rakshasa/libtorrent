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

#ifndef LIBTORRENT_DATA_HASH_TORRENT_H
#define LIBTORRENT_DATA_HASH_TORRENT_H

#include <cinttypes>
#include <functional>
#include <string>
#include <rak/priority_queue_default.h>

#include "data/chunk_handle.h"
#include "torrent/utils/ranges.h"

namespace torrent {

class ChunkList;

class HashTorrent {
public:
  typedef ranges<uint32_t> Ranges;

  typedef std::function<void (ChunkHandle)> slot_chunk_handle;

  HashTorrent(ChunkList* c);
  ~HashTorrent() { clear(); }

  bool                start(bool try_quick);
  void                clear();

  bool                is_checking()                          { return m_outstanding >= 0; }
  bool                is_checked();

  void                confirm_checked();

  Ranges&             hashing_ranges()                       { return m_ranges; }
  uint32_t            position() const                       { return m_position; }
  uint32_t            outstanding() const                    { return m_outstanding; }

  int                 error_number() const                   { return m_errno; }

  slot_chunk_handle&  slot_check_chunk() { return m_slot_check_chunk; }

  rak::priority_item& delay_checked()                        { return m_delayChecked; }

  void                receive_chunkdone(uint32_t index);
  void                receive_chunk_cleared(uint32_t index);
  
private:
  void                queue(bool quick);

  unsigned int        m_position;
  int                 m_outstanding;
  Ranges              m_ranges;

  int                 m_errno;

  ChunkList*          m_chunk_list;

  slot_chunk_handle   m_slot_check_chunk;

  rak::priority_item  m_delayChecked;
};

}

#endif

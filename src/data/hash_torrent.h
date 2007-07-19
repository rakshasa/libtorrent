// libTorrent - BitTorrent library
// Copyright (C) 2005-2007, Jari Sundell
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

#include <string>
#include <inttypes.h>
#include <rak/functional.h>
#include <rak/ranges.h>
#include <rak/priority_queue_default.h>

#include "data/chunk_handle.h"

namespace torrent {

class ChunkList;
class DownloadWrapper;

class HashTorrent {
public:
  typedef rak::ranges<uint32_t> Ranges;

  typedef rak::mem_fun1<DownloadWrapper, void, ChunkHandle>        slot_check_type;
  typedef rak::mem_fun1<DownloadWrapper, void, const std::string&> slot_error_type;
  
  HashTorrent(ChunkList* c);
  ~HashTorrent() { clear(); }

  bool                start(bool tryQuick);
  void                clear();

  bool                is_checking()                          { return m_outstanding >= 0; }
  bool                is_checked();

  void                confirm_checked();

  Ranges&             ranges()                               { return m_ranges; }
  uint32_t            position() const                       { return m_position; }
  uint32_t            outstanding() const                    { return m_outstanding; }

  int                 error_number() const                   { return m_errno; }

  void                slot_check(slot_check_type s)          { m_slotCheck = s; }
//   void                slot_error(slot_error_type s)          { m_slotError = s; }

  rak::priority_item& delay_checked()                        { return m_delayChecked; }

  void                receive_chunkdone();
  void                receive_chunk_cleared(uint32_t index);
  
private:
  void                queue(bool quick);

  unsigned int        m_position;
  int                 m_outstanding;
  Ranges              m_ranges;

  int                 m_errno;

  ChunkList*          m_chunkList;

  slot_check_type     m_slotCheck;
  slot_error_type     m_slotError;

  rak::priority_item  m_delayChecked;
};

}

#endif

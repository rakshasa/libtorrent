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

#ifndef LIBTORRENT_DOWNLOAD_CHUNK_SELECTOR_H
#define LIBTORRENT_DOWNLOAD_CHUNK_SELECTOR_H

#include <inttypes.h>
#include <rak/ranges.h>

#include "utils/bitfield.h"
#include "utils/bitfield_counter.h"

namespace torrent {

// This class is responsible for deciding on which chunk index to
// download next based on the peer's bitfield. It keeps its own
// bitfield which starts out as a copy of Content::bitfield but sets
// chunks that are being downloaded.
//
// When updating Content::bitfield, make sure you update this bitfield
// and unmark any chunks in Delegator.

class PeerChunks;

class ChunkSelector {
public:
  typedef rak::ranges<uint32_t> PriorityRanges;

  static const uint32_t invalid_chunk = ~(uint32_t)0;

  BitField*           bitfield()                    { return &m_bitfield; }
  BitFieldCounter*    bitfield_counter()            { return &m_bitfieldCounter; }

  PriorityRanges*     high_priority()               { return &m_highPriority; }
  PriorityRanges*     normal_priority()             { return &m_normalPriority; }

  // Call this once you've modified the bitfield or priorities to
  // update cached information. This must be called once before using
  // find.
  void                update();

  uint32_t            find(PeerChunks* peerChunks, bool highPriority);

  void                enable_index(uint32_t index);
  void                disable_index(uint32_t index);

  void                insert_peer(PeerChunks* peerChunks);
  void                erase_peer(PeerChunks* peerChunks);

private:
  BitField            m_bitfield;
  BitFieldCounter     m_bitfieldCounter;
  
  PriorityRanges      m_highPriority;
  PriorityRanges      m_normalPriority;

  uint32_t            m_position;
};

}

#endif  

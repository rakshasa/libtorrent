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

#ifndef LIBTORRENT_DOWNLOAD_CHUNK_SELECTOR_H
#define LIBTORRENT_DOWNLOAD_CHUNK_SELECTOR_H

#include <inttypes.h>
#include <rak/ranges.h>
#include <rak/partial_queue.h>

#include "torrent/bitfield.h"

namespace torrent {

// This class is responsible for deciding on which chunk index to
// download next based on the peer's bitfield. It keeps its own
// bitfield which starts out as a copy of Content::bitfield but sets
// chunks that are being downloaded.
//
// When updating Content::bitfield, make sure you update this bitfield
// and unmark any chunks in Delegator.

class ChunkStatistics;
class PeerChunks;

class ChunkSelector {
public:
  typedef rak::ranges<uint32_t> PriorityRanges;

  static const uint32_t invalid_chunk = ~(uint32_t)0;

  bool                empty() const                 { return size() == 0; }
  uint32_t            size() const                  { return m_bitfield.size_bits(); }

  const Bitfield*     bitfield()                    { return &m_bitfield; }

  PriorityRanges*     high_priority()               { return &m_highPriority; }
  PriorityRanges*     normal_priority()             { return &m_normalPriority; }

  // Initialize doesn't update the priority cache, so it is as if it
  // has empty priority ranges.
  void                initialize(Bitfield* bf, ChunkStatistics* cs);
  void                cleanup();

  // Call this once you've modified the bitfield or priorities to
  // update cached information. This must be called once before using
  // find.
  void                update_priorities();

  uint32_t            find(PeerChunks* pc, bool highPriority);

  bool                is_wanted(uint32_t index) const;

  // Call this to set the index as being downloaded, finished etc,
  // thus ignored. Propably should find a better name for this.
  void                using_index(uint32_t index);
  void                not_using_index(uint32_t index);

  // The caller must ensure that the chunk index is valid and has not
  // been set already.
  //
  // The user only needs to call this when it needs to know whetever
  // it should become interested, or if it is in the process of
  // downloading.
  //
  // Returns whetever we're interested in that piece.
  bool                received_have_chunk(PeerChunks* pc, uint32_t index);

private:
  bool                search_linear(const Bitfield* bf, rak::partial_queue* pq, PriorityRanges* ranges, uint32_t first, uint32_t last);
  inline bool         search_linear_range(const Bitfield* bf, rak::partial_queue* pq, uint32_t first, uint32_t last);
  inline bool         search_linear_byte(rak::partial_queue* pq, uint32_t index, Bitfield::value_type wanted);

//   inline uint32_t     search_rarest(const Bitfield* bf, PriorityRanges* ranges, uint32_t first, uint32_t last);
//   inline uint32_t     search_rarest_range(const Bitfield* bf, uint32_t first, uint32_t last);
//   inline uint32_t     search_rarest_byte(uint8_t wanted);

  void                advance_position();

  Bitfield            m_bitfield;
  ChunkStatistics*    m_statistics;
  
  PriorityRanges      m_highPriority;
  PriorityRanges      m_normalPriority;

  rak::partial_queue  m_sharedQueue;

  uint32_t            m_position;
};

}

#endif  

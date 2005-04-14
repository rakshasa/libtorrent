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
// Contact:  Jari Sundell <jaris@ifi.uio.no>
//
//           Skomakerveien 33
//           3185 Skoppum, NORWAY

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cstring>
#include <algo/algo.h>

#include "torrent/exceptions.h"
#include "download/download_state.h"

#include "settings.h"

using namespace algo;

namespace torrent {

void
DownloadState::update_endgame() {
  if (m_content.get_chunks_completed() + m_slotDelegatedChunks() + m_settings->endgameBorder
      >= get_chunk_total())
    m_slotSetEndgame(true);
}

void
DownloadState::chunk_done(unsigned int index) {
  Storage::Chunk c = m_content.get_storage().get_chunk(index, MemoryChunk::prot_read);

  if (!c.is_valid())
    throw internal_error("DownloadState::chunk_done(...) called with an index we couldn't retrive from storage");

  m_slotHashCheckAdd(c);
}

uint64_t
DownloadState::bytes_left() {
  uint64_t left = m_content.get_size() - m_content.get_bytes_completed();

  if (left > ((uint64_t)1 << 60))
    throw internal_error("DownloadState::bytes_left() is too large"); 

  if (m_content.get_chunks_completed() == get_chunk_total() && left != 0)
    throw internal_error("DownloadState::bytes_left() has an invalid size"); 

  return left;
}

void
DownloadState::receive_hash_done(Storage::Chunk c, std::string hash) {
  if (std::memcmp(hash.c_str(), m_content.get_hash_c(c->get_index()), 20) == 0) {

    m_content.mark_done(c->get_index());
    m_signalChunkPassed.emit(c->get_index());

    update_endgame();

  } else {
    m_signalChunkFailed.emit(c->get_index());
  }
}  

}

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

#ifndef LIBTORRENT_DOWNLOAD_STATE_H
#define LIBTORRENT_DOWNLOAD_STATE_H

#include <sigc++/signal.h>
#include <algo/ref_anchored.h>

#include "data/storage_chunk.h"
#include "content/content.h"
#include "utils/bitfield_counter.h"

namespace torrent {

class DownloadSettings;

// Here goes all those things that Peer* and Delegator needs.
class DownloadState {
public:
  typedef algo::RefAnchored<StorageChunk> Chunk;
  
  DownloadState() : m_settings(NULL) {}

  DownloadSettings&   get_settings()                            { return *m_settings; }
  void                set_settings(DownloadSettings* s)         { m_settings = s; }

  Content&            get_content()                             { return m_content; }
  BitFieldCounter&    get_bitfield_counter()                    { return m_bfCounter; }

  // Schedules chunk for hash check.
  void                chunk_done(unsigned int index);

  uint32_t            get_chunk_total()                         { return m_content.get_storage().get_chunk_total(); }
  uint64_t            bytes_left();

  void                update_endgame();

  void                receive_hash_done(Storage::Chunk c, std::string hash);

  typedef sigc::signal1<void, uint32_t>           SignalChunk;
  typedef sigc::signal1<void, const std::string&> SignalString;

  typedef sigc::slot1<void, bool>         SlotSetEndgame;
  typedef sigc::slot0<uint32_t>           SlotDelegatedChunks;
  typedef sigc::slot1<void, Chunk>        SlotHashCheckAdd;

  SignalChunk&                            signal_chunk_passed()  { return m_signalChunkPassed; }
  SignalChunk&                            signal_chunk_failed()  { return m_signalChunkFailed; }

  SignalString&                           signal_storage_error() { return m_signalStorageError; }

  void slot_set_endgame(SlotSetEndgame s)                        { m_slotSetEndgame = s; }
  void slot_delegated_chunks(SlotDelegatedChunks s)              { m_slotDelegatedChunks = s; }
  void slot_hash_check_add(SlotHashCheckAdd s)                   { m_slotHashCheckAdd = s; }

private:
  // Disable copy ctor and assignment
  DownloadState(const DownloadState&);
  void operator = (const DownloadState&);

  DownloadSettings*   m_settings;
  Content             m_content;
  BitFieldCounter     m_bfCounter;

  SignalChunk         m_signalChunkPassed;
  SignalChunk         m_signalChunkFailed;
  SignalString        m_signalStorageError;

  SlotSetEndgame      m_slotSetEndgame;
  SlotDelegatedChunks m_slotDelegatedChunks;
  SlotHashCheckAdd    m_slotHashCheckAdd;
};

}

#endif

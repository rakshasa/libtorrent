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

#ifndef LIBTORRENT_HASH_TORRENT_H
#define LIBTORRENT_HASH_TORRENT_H

#include <string>
#include <sigc++/signal.h>
#include <algo/ref_anchored.h>

#include "storage_chunk.h"
#include "utils/ranges.h"

namespace torrent {

class Storage;
class HashQueue;

class HashTorrent {
public:
  typedef algo::RefAnchored<StorageChunk>           Chunk;
  typedef sigc::signal0<void>                       SignalTorrentDone;
  typedef sigc::signal2<void, Chunk, std::string>   SignalChunkDone;
  
  HashTorrent(const std::string& id, Storage* s);
  ~HashTorrent() { stop(); }

  void                start();
  void                stop();

  bool                is_checking()                 { return m_outstanding; }

  Ranges&             get_ranges()                  { return m_ranges; }

  HashQueue*          get_queue()                   { return m_queue; }
  void                set_queue(HashQueue* q)       { m_queue = q; }

  SignalTorrentDone&  signal_torrent()              { return m_signalTorrent; }
  SignalChunkDone&    signal_chunk()                { return m_signalChunk; }

private:
  void                queue();

  void                receive_chunkdone(Chunk c, std::string hash);
  
  std::string         m_id;
  
  unsigned int        m_position;
  unsigned int        m_outstanding;
  Ranges              m_ranges;

  Storage*            m_storage;
  HashQueue*          m_queue;

  SignalTorrentDone   m_signalTorrent;
  SignalChunkDone     m_signalChunk;
};

}

#endif

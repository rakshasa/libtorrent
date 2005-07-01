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

#ifndef LIBTORRENT_DATA_HASH_TORRENT_H
#define LIBTORRENT_DATA_HASH_TORRENT_H

#include <string>
#include <sigc++/signal.h>

#include "storage_chunk.h"
#include "utils/ranges.h"
#include "utils/ref_anchored.h"

namespace torrent {

class Storage;
class HashQueue;

class HashTorrent {
public:
  typedef RefAnchored<StorageChunk>                 Chunk;
  typedef sigc::signal0<void>                       Signal;
  typedef sigc::signal2<void, Chunk, std::string>   SignalChunkDone;
  
  HashTorrent(const std::string& id, Storage* s);
  ~HashTorrent() { clear(); }

  void                start();
  void                stop();

  void                clear();

  bool                is_checking()                 { return m_outstanding >= 0; }

  Ranges&             get_ranges()                  { return m_ranges; }

  HashQueue*          get_queue()                   { return m_queue; }
  void                set_queue(HashQueue* q)       { m_queue = q; }

  Signal&             signal_torrent()              { return m_signalTorrent; }
  SignalChunkDone&    signal_chunk()                { return m_signalChunk; }

private:
  void                queue();

  void                receive_chunkdone(Chunk c, std::string hash);
  
  std::string         m_id;
  
  unsigned int        m_position;
  int                 m_outstanding;
  Ranges              m_ranges;

  Storage*            m_storage;
  HashQueue*          m_queue;

  Signal              m_signalTorrent;
  SignalChunkDone     m_signalChunk;
};

}

#endif

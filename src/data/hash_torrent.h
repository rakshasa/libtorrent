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
  typedef algo::RefAnchored<StorageChunk>         Chunk;
  typedef sigc::signal0<void>                     SignalTorrentDone;
  typedef sigc::signal2<void, Chunk, std::string> SignalChunkDone;
  
  HashTorrent(const std::string& id, Storage* s, HashQueue* queue);
  ~HashTorrent() { stop(); }

  void                start();
  void                stop();

  bool                is_checking()         { return m_outstanding; }

  Ranges&             get_ranges()          { return m_ranges; }

  SignalTorrentDone   signal_torrent()      { return m_signalTorrent; }
  SignalChunkDone     signal_chunk()        { return m_signalChunk; }

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

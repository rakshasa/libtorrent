#ifndef LIBTORRENT_DOWNLOAD_STATE_H
#define LIBTORRENT_DOWNLOAD_STATE_H

#include "bitfield_counter.h"
#include "peer_info.h"
#include "rate.h"
#include "settings.h"
#include "content/content.h"
#include "data/hash_torrent.h"
#include "torrent/peer.h"

#include <list>
#include <sigc++/signal.h>

namespace torrent {

extern HashQueue hashQueue;
extern HashTorrent hashTorrent;

class PeerConnection;
class DownloadNet;

// Here goes all those things that Peer* and Delegator needs.
class DownloadState {
 public:
  DownloadState();
  ~DownloadState();

  PeerInfo&     me()              { return m_me; }
  std::string&  hash()            { return m_hash; }

  Content&      content()         { return m_content; }

  DownloadSettings& settings() { return m_settings; }
  const DownloadSettings& settings() const { return m_settings; }

  void chunk_done(unsigned int index);

  BitFieldCounter& bfCounter() { return m_bfCounter; }

  uint64_t bytes_left();

  // Incoming signals.
  void receive_hashdone(std::string id, Storage::Chunk c, std::string hash);

  typedef sigc::signal1<void, uint32_t> SignalChunk;

  SignalChunk&                          signal_chunk_passed()      { return m_signalChunkPassed; }
  SignalChunk&                          signal_chunk_failed()      { return m_signalChunkFailed; }

private:
  // Disable
  DownloadState(const DownloadState&);
  void operator = (const DownloadState&);

  PeerInfo m_me;
  std::string m_hash;
  
  Content m_content;

  DownloadSettings m_settings;

  BitFieldCounter m_bfCounter;

  SignalChunk            m_signalChunkPassed;
  SignalChunk            m_signalChunkFailed;
};

}

#endif

#ifndef LIBTORRENT_DELEGATOR_H
#define LIBTORRENT_DELEGATOR_H

#include <sigc++/signal.h>

#include "priority.h"
#include "delegator_chunk.h"

namespace torrent {

// Go out of engame mode if the last piece is failing, perhaps a
// panic mode of sorts?
class Delegator {
public:
  static const unsigned int slow_peer    = 0x01;
  static const unsigned int fast_peer    = 0x02;
  static const unsigned int start_mode   = 0x04;
  static const unsigned int endgame_mode = 0x08;
  
  typedef std::list<DelegatorChunk>         ChunkList;
  typedef sigc::signal1<bool, unsigned int> SignalChunkDone;
  typedef sigc::signal1<void, const Piece&> SignalPieceDone;

  Priority&        priority()          { return m_priority; }

  DelegatorPiece   delegate(const BitField& bf, int flags);

  // Add id here so we can avoid downloading from the same source?
  void             done(DelegatorPiece& p);

  SignalChunkDone& signal_chunk_done() { return m_signalChunkDone; }

private:
  Priority         m_priority;
  ChunkList        m_chunks;

  SignalChunkDone  m_signalChunkDone;
};

}

#endif

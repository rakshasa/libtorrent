#ifndef LIBTORRENT_DELEGATOR_H
#define LIBTORRENT_DELEGATOR_H

#include <sigc++/signal.h>

#include "piece.h"

namespace torrent {

class Delegator {
public:
  static const unsigned int slow_peer    = 0x01;
  static const unsigned int fast_peer    = 0x02;
  static const unsigned int start_mode   = 0x04;
  static const unsigned int endgame_mode = 0x08;
  
  typedef sigc::signal1<bool, unsigned int> SignalChunkDone;
  typedef sigc::signal1<void, const Piece&> SignalPieceDone;

  // Queue size thing?
  bool delegate(const std::string& id, const BitField& bf, int flags, Piece& piece);

  Priority& priority();

  void done(const std::string& id, Piece& piece);
  void cancel(const std::string& id);

  SignalChunkDone& signal_chunk_done();

private:

};

}

#endif

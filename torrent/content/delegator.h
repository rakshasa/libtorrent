#ifndef LIBTORRENT_DELEGATOR_H
#define LIBTORRENT_DELEGATOR_H

#include <string>
#include <list>
#include <sigc++/signal.h>

#include "bitfield.h"
#include "piece.h"
#include "content/delegator_select.h"
#include "content/delegator_chunk.h"

namespace torrent {

class DownloadState;

class Delegator {
public:
  typedef std::list<DelegatorChunk*>        Chunks;
  typedef sigc::signal1<void, unsigned int> SignalChunkDone;

  Delegator() : m_state(NULL) {}
  Delegator(DownloadState* ds) : m_state(ds) { }
  ~Delegator();

  bool               interested(const BitField& bf);
  bool               interested(int index);

  DelegatorReservee* delegate(const BitField& bf, int affinity);
  bool               downloading(DelegatorReservee& r);
  void               finished(DelegatorReservee& r);

  void               cancel(DelegatorReservee& r);

  void               done(int index);
  void               redo(int index);

  Chunks&            chunks() { return m_chunks; }
  DelegatorSelect&   select() { return m_select; }

  SignalChunkDone&   signal_chunk_done() { return m_signalChunkDone; }

private:
  // Start on a new chunk, returns .end() if none possible. bf is
  // remote peer's bitfield.
  DelegatorPiece*    new_chunk(const BitField& bf);

  DelegatorPiece*    find_piece(const Piece& p);

  bool               all_state(int index, DelegatorState s);

  // TODO: Get rid of m_state.
  DownloadState*     m_state;
  Chunks             m_chunks;
  DelegatorSelect    m_select;

  SignalChunkDone    m_signalChunkDone;
};

} // namespace torrent

#endif // LIBTORRENT_DELEGATOR_H

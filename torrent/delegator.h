#ifndef LIBTORRENT_DELEGATOR_H
#define LIBTORRENT_DELEGATOR_H

#include "bitfield.h"
#include "piece.h"
#include "content/delegator_select.h"
#include "content/delegator_piece.h"

#include <string>
#include <list>

namespace torrent {

class DownloadState;

class Delegator {
public:
  // Internal, not to be confused with the chunk.h
  struct Chunk {
    Chunk(int index) : m_index(index) {}

    int m_index;
    std::list<DelegatorPiece*> m_pieces;
  };
  
  typedef std::list<Chunk> Chunks;

  Delegator() : m_state(NULL) {}
  Delegator(DownloadState* ds) :
    m_state(ds) { }

  bool interested(const BitField& bf);
  bool interested(int index);

  DelegatorReservee* delegate(const BitField& bf, int affinity);
  bool downloading(DelegatorReservee& r);
  bool finished(DelegatorReservee& r);

  void cancel(DelegatorReservee& r, bool clear);

  void done(int index);
  void redo(int index);

  Chunks& chunks() { return m_chunks; }

  DelegatorSelect& select() { return m_select; }

private:
  // Start on a new chunk, returns .end() if none possible. bf is
  // remote peer's bitfield.
  DelegatorPiece* newChunk(const BitField& bf);

  int findChunk(const BitField& bf);

  DownloadState* m_state;
  Chunks m_chunks;
  DelegatorSelect m_select;
};

} // namespace torrent

#endif // LIBTORRENT_DELEGATOR_H

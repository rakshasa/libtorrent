#ifndef LIBTORRENT_DELEGATOR_H
#define LIBTORRENT_DELEGATOR_H

#include <string>
#include <vector>
#include <sigc++/signal.h>

#include "bitfield.h"
#include "piece.h"
#include "content/delegator_select.h"
#include "content/delegator_chunk.h"

namespace torrent {

class DownloadState;

class Delegator {
public:
  typedef std::vector<DelegatorChunk*>        Chunks;
  typedef sigc::signal1<void, unsigned int>   SignalChunkDone;
  typedef sigc::slot1<uint32_t, unsigned int> SlotChunkSize;

  Delegator() : m_chunksize(0) { }
  ~Delegator();

  bool               interested(const BitField& bf);
  bool               interested(int index);

  DelegatorReservee* delegate(const BitField& bf, int affinity);
  bool               downloading(DelegatorReservee& r);
  void               finished(DelegatorReservee& r);

  void               cancel(DelegatorReservee& r);

  void               done(int index);
  void               redo(int index);

  Chunks&            get_chunks()                         { return m_chunks; }
  DelegatorSelect&   get_select()                         { return m_select; }

  SignalChunkDone&   signal_chunk_done()                  { return m_signalChunkDone; }

  void               slot_chunk_size(SlotChunkSize s)     { m_slotChunkSize = s; }

private:
  // Start on a new chunk, returns .end() if none possible. bf is
  // remote peer's bitfield.
  DelegatorPiece*    new_chunk(const BitField& bf);
  DelegatorPiece*    find_piece(const Piece& p);

  bool               all_state(int index, DelegatorState s);

  uint64_t           m_totalsize;
  uint32_t           m_chunksize;

  Chunks             m_chunks;
  DelegatorSelect    m_select;

  SignalChunkDone    m_signalChunkDone;
  SlotChunkSize      m_slotChunkSize;
};

} // namespace torrent

#endif // LIBTORRENT_DELEGATOR_H

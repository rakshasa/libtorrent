#ifndef LIBTORRENT_DELEGATOR_H
#define LIBTORRENT_DELEGATOR_H

#include <string>
#include <vector>
#include <sigc++/signal.h>

#include "utils/bitfield.h"
#include "data/piece.h"
#include "content/delegator_select.h"
#include "content/delegator_chunk.h"

namespace torrent {

class DownloadState;

class Delegator {
public:
  typedef std::vector<DelegatorChunk*>        Chunks;
  typedef sigc::signal1<void, unsigned int>   SignalChunkDone;
  typedef sigc::slot1<uint32_t, unsigned int> SlotChunkSize;

  Delegator() : m_aggressive(false) { }
  ~Delegator() { clear(); }

  void               clear();

  DelegatorReservee* delegate(const BitField& bf, int affinity);

  void               finished(DelegatorReservee& r);

  void               done(int index);
  void               redo(int index);

  Chunks&            get_chunks()                         { return m_chunks; }
  DelegatorSelect&   get_select()                         { return m_select; }

  bool               get_aggressive()                     { return m_aggressive; }
  void               set_aggressive(bool a)               { m_aggressive = a; }

  SignalChunkDone&   signal_chunk_done()                  { return m_signalChunkDone; }

  void               slot_chunk_size(SlotChunkSize s)     { m_slotChunkSize = s; }

private:
  // Start on a new chunk, returns .end() if none possible. bf is
  // remote peer's bitfield.
  DelegatorPiece*    new_chunk(const BitField& bf, Priority::Type p);
  DelegatorPiece*    find_piece(const Piece& p);

  bool               all_finished(int index);

  bool               delegate_piece(DelegatorChunk& c, DelegatorPiece*& p);
  bool               delegate_aggressive(DelegatorChunk& c, DelegatorPiece*& p, uint16_t& overlapped);

  bool               m_aggressive;

  Chunks             m_chunks;
  DelegatorSelect    m_select;

  SignalChunkDone    m_signalChunkDone;
  SlotChunkSize      m_slotChunkSize;
};

} // namespace torrent

#endif // LIBTORRENT_DELEGATOR_H

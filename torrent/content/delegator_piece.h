#ifndef LIBTORRENT_DELEGATOR_PIECE_H
#define LIBTORRENT_DELEGATOR_PIECE_H

#include "piece.h"
#include "delegator_state.h"

namespace torrent {

// Note that DelegatorReservee and DelegatorPiece do not safely
// destruct, copy and stuff like that. You need to manually make
// sure you disconnect the reservee from the piece. I don't see any
// point in making this reference counted and stuff like that since
// we have a clear place of origin and end of the objects.

class DelegatorReservee;

class DelegatorPiece {
public:
  DelegatorPiece(const Piece& p) : m_piece(p), m_state(DELEGATOR_NONE), m_reservee(NULL) {}
  ~DelegatorPiece() { clear(); }
  
  void               clear();

  const Piece&       get_piece()                        { return m_piece; }
  void               set_piece(const Piece& p)          { m_piece = p; }

  DelegatorState     get_state()                        { return m_state; }
  void               set_state(DelegatorState s)        { m_state = s; }
  
  DelegatorReservee* get_reservee()                     { return m_reservee; }
  void               set_reservee(DelegatorReservee* r) { m_reservee = r; }

private:
  DelegatorPiece(const DelegatorPiece&);
  void operator = (const DelegatorPiece&);

  Piece              m_piece;
  DelegatorState     m_state;
  DelegatorReservee* m_reservee;
};

}

#endif

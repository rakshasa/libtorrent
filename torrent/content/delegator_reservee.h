#ifndef LIBTORRENT_DELEGATOR_RESERVEE_H
#define LIBTORRENT_DELEGATOR_RESERVEE_H

#include "delegator_state.h"
#include "delegator_piece.h"

namespace torrent {

class DelegatorPiece;

class DelegatorReservee {
public:
  friend class DelegatorPiece;

  DelegatorReservee(DelegatorPiece p = NULL) : m_piece(NULL) {}

  bool              is_valid()     { return m_piece; }

  const Piece&      get_piece()    { return m_piece->get_piece(); }
  DelegatorState    get_state()    { return m_piece->get_state(); }
  DelegatorPiece*   get_parent()   { return m_piece; }

  // Setting state to NONE or FINISHED automagically disconnects us
  // from the piece. Make sure it's done before killing the last
  // object cloned from this.
  void              set_state(DelegatorState s);
  void              set_parent(DelegatorPiece* p);

private:
  DelegatorPiece* m_piece;
};

}

#endif

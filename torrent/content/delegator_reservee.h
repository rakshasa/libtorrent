#ifndef LIBTORRENT_DELEGATOR_RESERVEE_H
#define LIBTORRENT_DELEGATOR_RESERVEE_H

#include "delegator_state.h"

namespace torrent {

class DelegatorReservee {
public:
  friend class DelegatorPiece;

  DelegatorReservee(DelegatorPiece p = NULL) : m_piece(NULL) {}

  bool              is_valid()     { return m_piece; }

  const Piece&      get_piece();
  DelegatorPiece*   get_parent();
  DelegatorState    get_state();

  // Setting state to NONE or FINISHED automagically disconnects us
  // from the piece. Make sure it's done before killing the last
  // object cloned from this.
  void set_state(DelegatorState s);

private:
  DelegatorPiece* m_piece;
};

}

#ifndef LIBTORRENT_DELEGATOR_RESERVEE_H
#define LIBTORRENT_DELEGATOR_RESERVEE_H

#include "delegator_state.h"
#include "delegator_piece.h"

namespace torrent {

class DelegatorPiece;

// Disconnect on NONE or FINISHED. If someone else takes over, then
// we won't be valid anymore. This should never happen unless in the endgame
// mode, and STALLED should be killed off after a few minutes. Therefor check
// for is_valid() before every write. Only allow pointers of this to be passed
// around since DelegatorPiece holds a pointer to this.

// Make sure state is set to NONE or FINISHED before deleting this object.

class DelegatorReservee {
public:
  friend class DelegatorPiece;

  DelegatorReservee(DelegatorPiece* p = NULL) : m_piece(NULL) {}
  ~DelegatorReservee();

  bool              is_valid()     { return m_piece; }

  const Piece&      get_piece()    { return m_piece->get_piece(); }
  DelegatorState    get_state()    { return m_piece->get_state(); }
  DelegatorPiece*   get_parent()   { return m_piece; }

  void              set_state(DelegatorState s);

private:
  DelegatorReservee(const DelegatorReservee&);
  void operator = (const DelegatorReservee&);

  DelegatorPiece* m_piece;
};

}

#endif

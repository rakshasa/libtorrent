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

  DelegatorReservee(DelegatorPiece* p = NULL);
  ~DelegatorReservee();

  bool              is_valid() const   { return m_parent; }

  const Piece&      get_piece() const  { return m_piece; }
  DelegatorState    get_state() const  { return m_parent ? m_parent->get_state() : DELEGATOR_NONE; }
  DelegatorPiece*   get_parent() const { return m_parent; }

  void              set_state(DelegatorState s);

private:
  DelegatorReservee(const DelegatorReservee&);
  void operator = (const DelegatorReservee&);

  DelegatorPiece* m_parent;
  Piece           m_piece;
};

}

#endif

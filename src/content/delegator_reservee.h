#ifndef LIBTORRENT_DELEGATOR_RESERVEE_H
#define LIBTORRENT_DELEGATOR_RESERVEE_H

#include <inttypes.h>
#include "delegator_piece.h"

namespace torrent {

class DelegatorReservee {
public:
  ~DelegatorReservee() { invalidate(); }

  void              invalidate();

  bool              is_valid() const          { return m_parent; }
  bool              is_stalled() const        { return m_stalled; }

  void              set_stalled(bool b);

  const Piece&      get_piece() const         { return m_parent->get_piece(); }
  DelegatorPiece*   get_parent() const        { return m_parent; }

  uint32_t          get_stall_counter() const { return m_stallCounter; }

protected:
  friend class DelegatorPiece;

  DelegatorReservee(DelegatorPiece* p = NULL) : m_parent(p), m_stalled(false) {}

private:
  DelegatorReservee(const DelegatorReservee&);
  void operator = (const DelegatorReservee&);

  DelegatorPiece* m_parent;
  uint32_t        m_stallCounter;
  bool            m_stalled;
};

}

#endif

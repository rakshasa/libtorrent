#ifndef LIBTORRENT_DELEGATOR_PIECE_H
#define LIBTORRENT_DELEGATOR_PIECE_H

#include "piece.h"
#include "delegator_state.h"

namespace torrent {

class DelegatorReservee;

class DelegatorPiece {
public:
  DelegatorPiece() : m_state(NONE), m_reserve(NULL) {}
  ~DelegatorPiece() { clear(); }
  
  DelegatorState     get_state()                        { return m_state; }
  void               set_state(DelegatorState s)        { m_state = s; }
  
  DelegatorReservee* get_reservee()                     { return m_reservee; }
  void               set_reservee(DelegatorReservee* r) { return m_reservee = r; }

  void               clear();

private:
  DelegatorPiece(const DelegatorPiece&);
  void operator = (const DelegatorPiece&);

  DelegatorState     m_state;
  DelegatorReservee* m_reservee;
};

}

#endif

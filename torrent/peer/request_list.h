#ifndef LIBTORRENT_REQUEST_LIST_H
#define LIBTORRENT_REQUEST_LIST_H

#include <deque>
#include "content/delegator_reservee.h"

namespace torrent {

class BitField;
class Delegator;

class RequestList {
public:
  typedef std::deque<DelegatorReservee*> ReserveeList;

  RequestList(Delegator* d, BitField* bf);
  ~RequestList()                              { cancel(); }

  // Some parameters here, like how fast we are downloading and stuff
  // when we start considering those.
  bool               delegate();

  void               cancel();
  //void              stall();

  bool               downloading(const Piece& p);
  bool               finished();

  bool               is_downloading()         { return m_downloading; }

  unsigned int       get_size()               { return m_reservees.size(); }
  const Piece&       get_piece()              { return m_downloading->get_piece(); }

private:
  void               delete_range(ReserveeList::iterator end);

  Delegator*         m_delegator;
  BitField*          m_bitfield;

  DelegatorReservee* m_downloading;
  ReserveeList       m_reservees;
};

}

#endif

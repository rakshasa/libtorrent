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

  RequestList() : m_delegator(NULL), m_bitfield(NULL), m_downloading(false) {}
  ~RequestList()                                                            { cancel(); }

  // Some parameters here, like how fast we are downloading and stuff
  // when we start considering those.
  const Piece*       delegate();

  void               cancel();
  void               stall(); // Just calls cancel for now

  bool               downloading(const Piece& p);
  void               finished();

  bool               is_downloading()                 { return m_downloading; }

  bool               has_index(unsigned int i);

  unsigned int       get_size()                       { return m_reservees.size(); }

  const Piece&       get_piece()                      { return m_reservees.front()->get_piece(); }
  const Piece&       get_queued_piece(unsigned int i) { return m_reservees[i]->get_piece(); }

  void               set_delegator(Delegator* d)      { m_delegator = d; }
  void               set_bitfield(const BitField* b)  { m_bitfield = b; }

private:
  void               cancel_range(ReserveeList::iterator end);

  Delegator*         m_delegator;
  const BitField*    m_bitfield;

  bool               m_downloading;
  ReserveeList       m_reservees;
};

}

#endif

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

  RequestList() :
    m_delegator(NULL),
    m_bitfield(NULL),
    m_affinity(-1),
    m_downloading(false) {}

  ~RequestList() { cancel(); }

  // Some parameters here, like how fast we are downloading and stuff
  // when we start considering those.
  const Piece*       delegate();

  // If is downloading, call skip before cancel.
  void               cancel();
  void               stall();
  void               skip();

  bool               downloading(const Piece& p);
  void               finished();

  bool               is_downloading()                 { return m_downloading; }
  bool               is_wanted()                      { return m_reservees.front()->is_valid(); }

  bool               has_index(uint32_t i);
  uint32_t           remove_invalid();

  size_t             get_size()                       { return m_reservees.size(); }

  const Piece&       get_piece()                      { return m_piece; }

  Piece              get_queued_piece(uint32_t i) {
    // TODO: Make this unnessesary?
    if (m_reservees[i]->is_valid())
      return m_reservees[i]->get_piece();
    else
      return Piece();
  }

  void               set_delegator(Delegator* d)      { m_delegator = d; }
  void               set_bitfield(const BitField* b)  { m_bitfield = b; }

private:
  void               cancel_range(ReserveeList::iterator end);

  Piece              m_piece;

  Delegator*         m_delegator;
  const BitField*    m_bitfield;

  int32_t            m_affinity;
  bool               m_downloading;

  ReserveeList       m_reservees;
  ReserveeList       m_canceled;
};

}

#endif

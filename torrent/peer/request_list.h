#ifndef LIBTORRENT_REQUEST_LIST_H
#define LIBTORRENT_REQUEST_LIST_H

#include <deque>

namespace torrent {

class BitField;
class Delegator;
class DelegatorReservee;

class RequestList {
public:
  typedef std::deque<DelegatorReservee*> ReserveeList;

  RequestList(Delegator* d, BitField* bf);
  ~RequestList() { cancel(); }

  // Some parameters here, like how fast we are downloading and stuff
  // when we start considering those.
  bool              delegate();

  void              cancel();
  void              stall();

  bool              recieved(const Piece& p);
  bool              downloading();
  bool              finished();

  unsigned int      get_size()                     { return m_reservees.size(); }
  const Piece&      get_piece(unsigned int index);

private:
  Delegator*   m_delegator;
  BitField*    m_bitfield;

  ReserveeList m_reservees;
};

}

#endif

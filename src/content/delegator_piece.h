#ifndef LIBTORRENT_DELEGATOR_PIECE_H
#define LIBTORRENT_DELEGATOR_PIECE_H

#include <vector>
#include <inttypes.h>

#include "piece.h"

namespace torrent {

// Note that DelegatorReservee and DelegatorPiece do not safely
// destruct, copy and stuff like that. You need to manually make
// sure you disconnect the reservee from the piece. I don't see any
// point in making this reference counted and stuff like that since
// we have a clear place of origin and end of the objects.

class DelegatorReservee;

class DelegatorPiece {
public:
  typedef std::vector<DelegatorReservee*> Reservees;

  DelegatorPiece() : m_finished(false), m_stalled(0) {}
  ~DelegatorPiece();

  DelegatorReservee* create();

  void               clear();

  bool               is_finished()                      { return m_finished; }
  void               set_finished(bool f)               { m_finished = f; }

  const Piece&       get_piece()                        { return m_piece; }
  void               set_piece(const Piece& p)          { m_piece = p; }

  uint32_t           get_reservees_size()               { return m_reservees.size(); }
  uint16_t           get_stalled()                      { return m_stalled; }
  uint16_t           get_not_stalled()                  { return m_reservees.size() - m_stalled; }

protected:
  friend class DelegatorReservee;

  void               remove(DelegatorReservee* r);

  void               inc_stalled()                      { ++m_stalled; }
  void               dec_stalled()                      { --m_stalled; }

private:
  DelegatorPiece(const DelegatorPiece&);
  void operator = (const DelegatorPiece&);

  Piece              m_piece;
  Reservees          m_reservees;

  bool               m_finished;
  uint16_t           m_stalled;
};

}

#endif

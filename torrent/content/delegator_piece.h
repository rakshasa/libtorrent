#ifndef LIBTORRENT_DELEGATOR_PIECE_H
#define LIBTORRENT_DELEGATOR_PIECE_H

#include "piece.h"

namespace torrent {

// *Always* clear before destroying the main object. Although this
// is very manual, making it automatic adds unnessesary overhead. This
// class is used in very few places so it should be easy to get correct
// behaviour.
class DelegatorPiece {
public:
  struct Info {
    Info(const Piece& p) : m_valid(0), m_active(0), m_piece(p) {}

    unsigned int m_valid;
    unsigned int m_active;
    Piece        m_piece;
  };

  DelegatorPiece() :
    m_active(false),
    m_ref(NULL) {}

  DelegatorPiece(Info* info) :
    m_active(false),
    m_ref(ref) {

    if (ref)
      m_ref->m_valid++;
  }

  const Piece& get_piece()        { return m_ref->m_piece; }
  
  bool         is_valid()         { return m_ref; }
  bool         is_active()        { return m_active; }

  void         set_active(bool a) {
    if (a != m_active) {
      m_active = a;
      m_ref->m_active += a ? 1 : -1;
    }
  }
  
  void         clear() {
    if (m_ref) {
      set_active(false);
      m_ref = NULL;
    }
  }

private:
  bool                m_active;
  DelegatorReference* m_ref;
};

}

#endif

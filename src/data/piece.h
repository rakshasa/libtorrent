#ifndef LIBTORRENT_PIECE_H
#define LIBTORRENT_PIECE_H

#include <inttypes.h>

namespace torrent {

class Piece {
public:
  Piece() : m_index(-1), m_offset(0), m_length(0) {}

  Piece(int32_t index, uint32_t offset, uint32_t length) :
    m_index(index), m_offset(offset), m_length(length) {}

  int32_t   get_index() const             { return m_index; }
  uint32_t  get_offset() const            { return m_offset; }
  uint32_t  get_length() const            { return m_length; }

  void      set_index(int32_t v)          { m_index = v; }
  void      set_offset(uint32_t v)        { m_offset = v; }
  void      set_length(uint32_t v)        { m_length = v; }

  bool operator == (const Piece& p) const {
    return m_index == p.m_index && m_offset == p.m_offset && m_length == p.m_length;
  }

private:
  int32_t   m_index;
  uint32_t  m_offset;
  uint32_t  m_length;
};

}

#endif // LIBTORRENT_PIECE_H


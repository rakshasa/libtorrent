#ifndef LIBTORRENT_PIECE_H
#define LIBTORRENT_PIECE_H

namespace torrent {

class Piece {
public:
  Piece() : m_index(-1),
	    m_offset(0),
	    m_length(0) {}
  Piece(int index,
	unsigned int offset,
	unsigned int length) : m_index(index),
			       m_offset(offset),
			       m_length(length) {}

  int& index() { return m_index; }
  unsigned int& offset() { return m_offset; }
  unsigned int& length() { return m_length; }

  int indexC() const { return m_index; }
  unsigned int offsetC() const { return m_offset; }
  unsigned int lengthC() const { return m_length; }

  bool operator == (const Piece& p) const {
    return
      m_index == p.m_index &&
      m_offset == p.m_offset &&
      m_length == p.m_length;
  }

private:
  int m_index;
  unsigned int m_offset;
  unsigned int m_length;
};

}

#endif // LIBTORRENT_PIECE_H


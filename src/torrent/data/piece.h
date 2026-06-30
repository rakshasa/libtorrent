#ifndef LIBTORRENT_PIECE_H
#define LIBTORRENT_PIECE_H

#include <torrent/common.h>

namespace RTORRENT_EXPORT torrent {

class Piece {
public:
  static constexpr uint32_t invalid_index = ~uint32_t();

  Piece()  = default;
  ~Piece() = default;
  Piece(const Piece&) = default;
  Piece& operator=(const Piece&) = default;

  Piece(uint32_t index, uint32_t offset, uint32_t length) :
    m_index(index), m_offset(offset), m_length(length) {}

  bool                is_valid() const              { return m_index != invalid_index; }

  uint32_t            index() const                 { return m_index; }
  void                set_index(uint32_t v)         { m_index = v; }

  uint32_t            offset() const                { return m_offset; }
  void                set_offset(uint32_t v)        { m_offset = v; }

  uint32_t            length() const                { return m_length; }
  void                set_length(uint32_t v)        { m_length = v; }

  bool operator == (const Piece& p) const { return m_index == p.m_index && m_offset == p.m_offset && m_length == p.m_length; }
  bool operator != (const Piece& p) const { return m_index != p.m_index || m_offset != p.m_offset || m_length != p.m_length; }

private:
  uint32_t m_index{invalid_index};
  uint32_t m_offset{};
  uint32_t m_length{};
};

} // namespace torrent

#endif

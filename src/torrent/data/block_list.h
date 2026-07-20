#ifndef LIBTORRENT_BLOCK_LIST_H
#define LIBTORRENT_BLOCK_LIST_H

#include <vector>
#include <torrent/common.h>
#include <torrent/data/block.h>
#include <torrent/data/piece.h>

namespace torrent {

class LIBTORRENT_EXPORT BlockList : private std::vector<Block> {
public:
  using size_type = uint32_t;
  using base_type = std::vector<Block>;

  using base_type::value_type;
  using base_type::reference;
  using base_type::difference_type;

  using base_type::iterator;
  using base_type::const_iterator;
  using base_type::size;
  using base_type::empty;

  using base_type::begin;
  using base_type::end;

  using base_type::operator[];

  BlockList(const Piece& piece, uint32_t blockLength);
  ~BlockList();
  BlockList(const BlockList&) = delete;
  BlockList& operator=(const BlockList&) = delete;

  bool                is_all_finished() const       { return m_finished == size(); }

  const Piece&        piece() const                 { return m_piece; }

  uint32_t            index() const                 { return m_piece.index(); }

  priority_enum       priority() const              { return m_priority; }
  void                set_priority(priority_enum p) { m_priority = p; }

  size_type           finished() const              { return m_finished; }
  void                inc_finished()                { m_finished++; }
  void                clear_finished()              { m_finished = 0; }

  uint32_t            failed() const                { return m_failed; }

  // Temporary, just increment for now.
  void                inc_failed()                  { m_failed++; }

  uint32_t            attempt() const               { return m_attempt; }
  void                set_attempt(uint32_t a)       { m_attempt = a; }

  // Set when the chunk was initially requested from a seeder. This
  // allows us to quickly determine if it is a suitable chunk to
  // request from another seeder, e.g by already knowing it is a rare
  // piece.
  bool                by_seeder() const             { return m_bySeeder; }
  void                set_by_seeder(bool state)     { m_bySeeder = state; }

  void                do_all_failed();

private:
  Piece               m_piece;
  priority_enum       m_priority{PRIORITY_OFF};

  size_type           m_finished{0};
  uint32_t            m_failed{0};
  uint32_t            m_attempt{0};

  bool                m_bySeeder{false};
};

} // namespace torrent

#endif

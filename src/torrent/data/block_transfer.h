// libTorrent - BitTorrent library
// Copyright (C) 2005-2011, Jari Sundell
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
// In addition, as a special exception, the copyright holders give
// permission to link the code of portions of this program with the
// OpenSSL library under certain conditions as described in each
// individual source file, and distribute linked combinations
// including the two.
//
// You must obey the GNU General Public License in all respects for
// all of the code used other than OpenSSL.  If you modify file(s)
// with this exception, you may extend this exception to your version
// of the file(s), but you are not obligated to do so.  If you do not
// wish to do so, delete this exception statement from your version.
// If you delete this exception statement from all source files in the
// program, then also delete it here.
//
// Contact:  Jari Sundell <jaris@ifi.uio.no>
//
//           Skomakerveien 33
//           3185 Skoppum, NORWAY

#ifndef LIBTORRENT_BLOCK_TRANSFER_H
#define LIBTORRENT_BLOCK_TRANSFER_H

#include <torrent/common.h>
#include <torrent/exceptions.h>
#include <torrent/data/piece.h>
#include <torrent/peer/peer_info.h>
#include <cstdlib>

namespace torrent {

class LIBTORRENT_EXPORT BlockTransfer {
public:
  static const uint32_t invalid_index = ~uint32_t();

  typedef PeerInfo* key_type;

  typedef enum {
    STATE_ERASED,
    STATE_QUEUED,
    STATE_LEADER,
    STATE_NOT_LEADER
  } state_type;

  BlockTransfer();
  ~BlockTransfer();

  // TODO: Do we need to also check for peer_info?...
  bool                is_valid() const              { return m_block != NULL; }

  bool                is_erased() const             { return m_state == STATE_ERASED; }
  bool                is_queued() const             { return m_state == STATE_QUEUED; }
  bool                is_leader() const             { return m_state == STATE_LEADER; }
  bool                is_not_leader() const         { return m_state == STATE_NOT_LEADER; }

  bool                is_finished() const           { return m_position == m_piece.length(); }

  key_type            peer_info()                   { return m_peer_info; }
  key_type            const_peer_info() const { return m_peer_info; }

  Block*              block()                       { return m_block; }
  const Block*        const_block() const           { return m_block; }

  const Piece&        piece() const                 { return m_piece; }
  uint32_t            index() const                 { return m_piece.index(); }
  state_type          state() const                 { return m_state; }
  int32_t             request_time() const          { return m_request_time; }

  // Adjust the position after any actions like erasing it from a
  // Block, but before if finishing.
  uint32_t            position() const              { return m_position; }
  uint32_t            stall() const                 { return m_stall; }
  uint32_t            failed_index() const          { return m_failedIndex; }

  void                set_peer_info(key_type p);
  void                set_block(Block* b)           { m_block = b; }
  void                set_piece(const Piece& p)     { m_piece = p; }
  void                set_state(state_type s)       { m_state = s; }
  void                set_request_time(int32_t t)   { m_request_time = t; }

  void                set_position(uint32_t p)      { m_position = p; }
  void                adjust_position(uint32_t p)   { m_position += p; }

  void                set_stall(uint32_t s)         { m_stall = s; }
  void                set_failed_index(uint32_t i)  { m_failedIndex = i; }

private:
  BlockTransfer(const BlockTransfer&);
  void operator = (const BlockTransfer&);

  key_type            m_peer_info;
  Block*              m_block;
  Piece               m_piece;

  state_type          m_state;
  int32_t             m_request_time;

  uint32_t            m_position;
  uint32_t            m_stall;
  uint32_t            m_failedIndex;
};

inline
BlockTransfer::BlockTransfer() :
  m_peer_info(NULL),
  m_block(NULL)
{
}

inline
BlockTransfer::~BlockTransfer() {
  if (m_block != NULL)
    throw internal_error("BlockTransfer::~BlockTransfer() block not NULL");

  if (m_peer_info != NULL)
    throw internal_error("BlockTransfer::~BlockTransfer() peer_info not NULL");
}

inline void
BlockTransfer::set_peer_info(key_type p) {
  if (m_peer_info != NULL)
    m_peer_info->dec_transfer_counter();

  m_peer_info = p;

  if (m_peer_info != NULL)
    m_peer_info->inc_transfer_counter();
}

}

#endif

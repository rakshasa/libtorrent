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

#ifndef LIBTORRENT_REQUEST_LIST_H
#define LIBTORRENT_REQUEST_LIST_H

#include <deque>

#include "torrent/data/block_transfer.h"
#include "utils/instrumentation.h"
#include "utils/queue_buckets.h"

#include "globals.h"

namespace torrent {

class PeerChunks;
class Delegator;

struct request_list_constants {
  static const int bucket_count = 4;

  static const torrent::instrumentation_enum instrumentation_added[bucket_count];
  static const torrent::instrumentation_enum instrumentation_moved[bucket_count];
  static const torrent::instrumentation_enum instrumentation_removed[bucket_count];
  static const torrent::instrumentation_enum instrumentation_total[bucket_count];

  template <typename Type>
  static void destroy(Type& obj);
};

class RequestList {
public:
  typedef torrent::queue_buckets<BlockTransfer*, request_list_constants> queues_type;

  static const int bucket_queued    = 0;
  static const int bucket_unordered = 1;
  static const int bucket_stalled   = 2;
  static const int bucket_choked    = 3;

  static const int timeout_remove_choked = 6;
  static const int timeout_choked_received = 60;
  static const int timeout_process_unordered = 60;

  RequestList();
  ~RequestList();

  // Some parameters here, like how fast we are downloading and stuff
  // when we start considering those.
  const Piece*         delegate();

  void                 stall_initial();
  void                 stall_prolonged();

  void                 choked();
  void                 unchoked();

  void                 clear();

  // The returned transfer must still be valid.
  bool                 downloading(const Piece& piece);

  void                 finished();
  void                 skipped();

  void                 transfer_dissimilar();

  bool                 is_downloading()                   { return m_transfer != NULL; }
  bool                 is_interested_in_active() const;

  // bool                 has_index(uint32_t i);

  const Piece&         next_queued_piece() const          { return m_queues.front(bucket_queued)->piece(); }

  bool                 queued_empty() const               { return m_queues.queue_empty(bucket_queued); }
  size_t               queued_size() const                { return m_queues.queue_size(bucket_queued); }
  bool                 unordered_empty() const            { return m_queues.queue_empty(bucket_unordered); }
  size_t               unordered_size() const             { return m_queues.queue_size(bucket_unordered); }
  bool                 stalled_empty() const              { return m_queues.queue_empty(bucket_stalled); }
  size_t               stalled_size() const               { return m_queues.queue_size(bucket_stalled); }
  bool                 choked_empty() const               { return m_queues.queue_empty(bucket_choked); }
  size_t               choked_size() const                { return m_queues.queue_size(bucket_choked); }

  uint32_t             pipe_size() const;
  uint32_t             calculate_pipe_size(uint32_t rate);

  void                 set_delegator(Delegator* d)       { m_delegator = d; }
  void                 set_peer_chunks(PeerChunks* b)    { m_peerChunks = b; }

  BlockTransfer*       transfer()                        { return m_transfer; }
  const BlockTransfer* transfer() const                  { return m_transfer; }

  const BlockTransfer* queued_front() const              { return m_queues.front(bucket_queued); }

private:
  void                 delay_remove_choked();

  void                 prepare_process_unordered(queues_type::iterator itr);
  void                 delay_process_unordered();

  Delegator*           m_delegator;
  PeerChunks*          m_peerChunks;

  BlockTransfer*       m_transfer;

  queues_type          m_queues;

  int32_t              m_affinity;

  rak::timer           m_last_choke;
  rak::timer           m_last_unchoke;
  size_t               m_last_unordered_position;

  rak::priority_item   m_delay_remove_choked;
  rak::priority_item   m_delay_process_unordered;
};

inline
RequestList::RequestList() :
  m_delegator(NULL),
  m_peerChunks(NULL),
  m_transfer(NULL),
  m_affinity(-1),
  m_last_unordered_position(0) {
  m_delay_remove_choked.slot() = std::tr1::bind(&RequestList::delay_remove_choked, this);
  m_delay_process_unordered.slot() = std::tr1::bind(&RequestList::delay_process_unordered, this);
}

// TODO: Make sure queued_size is never too small.
inline uint32_t
RequestList::pipe_size() const {
  return queued_size() + stalled_size() + unordered_size() / 4;
}

}

#endif

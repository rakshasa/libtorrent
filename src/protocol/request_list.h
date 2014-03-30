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

namespace torrent {

class PeerChunks;
class Delegator;

struct request_list_constants {
  static const int bucket_count = 2;

  static const torrent::instrumentation_enum instrumentation_added[bucket_count];
  static const torrent::instrumentation_enum instrumentation_removed[bucket_count];
  static const torrent::instrumentation_enum instrumentation_total[bucket_count];

  template <typename Type>
  static void destroy(Type& obj);
};

class RequestList {
public:
  typedef torrent::queue_buckets<BlockTransfer*, request_list_constants> queues_type;

  static const int bucket_queued = 0;
  static const int bucket_canceled = 1;

  RequestList() :
    m_delegator(NULL),
    m_peerChunks(NULL),
    m_transfer(NULL),
    m_affinity(-1) {}
  ~RequestList();

  // Some parameters here, like how fast we are downloading and stuff
  // when we start considering those.
  const Piece*         delegate();

  void                 stall_initial();
  void                 stall_prolonged();

  // If is downloading, call skip before cancel.
  void                 cancel();
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
  bool                 canceled_empty() const             { return m_queues.queue_empty(bucket_canceled); }
  size_t               canceled_size() const              { return m_queues.queue_size(bucket_canceled); }

  uint32_t             calculate_pipe_size(uint32_t rate);

  void                 set_delegator(Delegator* d)       { m_delegator = d; }
  void                 set_peer_chunks(PeerChunks* b)    { m_peerChunks = b; }

  BlockTransfer*       transfer()                        { return m_transfer; }
  const BlockTransfer* transfer() const                  { return m_transfer; }

  const BlockTransfer* queued_front() const              { return m_queues.front(bucket_queued); }

private:
  void                 cancel_range(queues_type::iterator end);

  Delegator*           m_delegator;
  PeerChunks*          m_peerChunks;

  BlockTransfer*       m_transfer;

  int32_t              m_affinity;

  queues_type          m_queues;
};

}

#endif

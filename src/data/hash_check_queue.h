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

#ifndef LIBTORRENT_DATA_HASH_CHECK_QUEUE_H
#define LIBTORRENT_DATA_HASH_CHECK_QUEUE_H

#include <deque>
#include <functional>
#include <pthread.h>

#include "rak/allocators.h"

namespace torrent {

class HashString;
class HashChunk;

class lt_cacheline_aligned HashCheckQueue : private std::deque<HashChunk*, rak::cacheline_allocator<HashChunk*> > {
public:
  typedef std::deque<HashChunk*, rak::cacheline_allocator<HashChunk*> > base_type;
  typedef std::function<void (HashChunk*, const HashString&)>      slot_chunk_handle;

  using base_type::iterator;

  using base_type::empty;
  using base_type::size;

  using base_type::begin;
  using base_type::end;
  using base_type::front;
  using base_type::back;
  
  HashCheckQueue();
  ~HashCheckQueue();

  // Guarded functions for adding new...

  void                push_back(HashChunk* node);
  void                perform();

  bool                remove(HashChunk* node);

  slot_chunk_handle&  slot_chunk_done() { return m_slot_chunk_done; }

private:
  slot_chunk_handle   m_slot_chunk_done;
  pthread_mutex_t     m_lock;
};

}

#endif

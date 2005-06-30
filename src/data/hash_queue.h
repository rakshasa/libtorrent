// libTorrent - BitTorrent library
// Copyright (C) 2005, Jari Sundell
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
// Contact:  Jari Sundell <jaris@ifi.uio.no>
//
//           Skomakerveien 33
//           3185 Skoppum, NORWAY

#ifndef LIBTORRENT_DATA_HASH_QUEUE_H
#define LIBTORRENT_DATA_HASH_QUEUE_H

#include <string>

#include "hash_queue_node.h"
#include "storage_chunk.h"

#include "utils/task.h"

namespace torrent {

class HashChunk;

// Calculating hash of incore memory is blindingly fast, it's always
// the loading from swap/disk that takes time. So with the exception
// of large resumed downloads, try to check the hash immediately. This
// helps us in getting as much done as possible while the pages are in
// memory.

class HashQueue : private std::list<HashQueueNode> {
public:
  typedef std::list<HashQueueNode> Base;

  typedef HashQueueNode::Chunk     Chunk;
  typedef HashQueueNode::SlotDone  SlotDone;

  using Base::iterator;

  using Base::empty;

  using Base::begin;
  using Base::end;

  HashQueue();
  ~HashQueue() { clear(); }

  void                push_back(Chunk c, SlotDone d, const std::string& id);

  bool                has(const std::string& id, uint32_t index);

  void                remove(const std::string& id);
  void                clear();

  void                work();

  uint32_t            get_read_ahead() const         { return m_readAhead; }
  void                set_read_ahead(uint32_t bytes) { m_readAhead = bytes; }

  uint32_t            get_interval() const           { return m_interval; }
  void                set_interval(uint32_t usec)    { m_interval = usec; }

  uint32_t            get_max_tries() const          { return m_maxTries; }
  void                set_max_tries(uint32_t tries)  { m_maxTries = tries; }

private:
  bool                check(bool force);

  inline void         willneed(int bytes);

  void                pop_front()                   { Base::front().clear(); Base::pop_front(); }

  uint16_t            m_tries;
  TaskItem            m_taskWork;

  uint32_t            m_readAhead;
  uint32_t            m_interval;
  uint32_t            m_maxTries;
};

}

#endif

// libTorrent - BitTorrent library
// Copyright (C) 2005-2006, Jari Sundell
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

#ifndef LIBTORRENT_DATA_HASH_QUEUE_H
#define LIBTORRENT_DATA_HASH_QUEUE_H

#include <string>
#include <list>

#include "hash_queue_node.h"
#include "chunk_handle.h"

#include "globals.h"

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
  typedef HashQueueNode::SlotDone  SlotDone;

  using Base::iterator;

  using Base::empty;

  using Base::begin;
  using Base::end;

  HashQueue();
  ~HashQueue() { clear(); }

  void                push_back(ChunkHandle handle, SlotDone d);

  bool                has(HashQueueNode::id_type id);
  bool                has(HashQueueNode::id_type id, uint32_t index);

  void                remove(HashQueueNode::id_type id);
  void                clear();

  void                work();

  uint32_t            read_ahead() const             { return m_readAhead; }
  void                set_read_ahead(uint32_t bytes) { m_readAhead = bytes; }

  uint32_t            interval() const               { return m_interval; }
  void                set_interval(uint32_t usec)    { m_interval = usec; }

  uint32_t            max_tries() const              { return m_maxTries; }
  void                set_max_tries(uint32_t tries)  { m_maxTries = tries; }

private:
  bool                check(bool force);

  inline void         willneed(int bytes);

  uint16_t            m_tries;
  rak::priority_item  m_taskWork;

  uint32_t            m_readAhead;
  uint32_t            m_interval;
  uint32_t            m_maxTries;
};

}

#endif

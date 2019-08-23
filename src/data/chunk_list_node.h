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

#ifndef LIBTORRENT_DATA_CHUNK_LIST_NODE_H
#define LIBTORRENT_DATA_CHUNK_LIST_NODE_H

#include <cinttypes>
#include <rak/timer.h>

namespace torrent {

class Chunk;

// ChunkNode can contain information like how long since it was last
// used, last synced, last checked with mincore and how many
// references there are to it.
//
// ChunkList will make sure all the nodes are cleaned up properly, so
// no dtor is needed.

class lt_cacheline_aligned ChunkListNode {
public:
  static const uint32_t invalid_index = ~uint32_t();

  ChunkListNode() :
    m_index(invalid_index),
    m_chunk(NULL),
    m_references(0),
    m_writable(0),
    m_blocking(0),
    m_asyncTriggered(false) {}

  bool                is_valid() const               { return m_chunk != NULL; }

  uint32_t            index() const                  { return m_index; }
  void                set_index(uint32_t idx)        { m_index = idx; }

  Chunk*              chunk() const                  { return m_chunk; }
  void                set_chunk(Chunk* c)            { m_chunk = c; }

  const rak::timer&   time_modified() const            { return m_timeModified; }
  void                set_time_modified(rak::timer t)  { m_timeModified = t; }

  const rak::timer&   time_preloaded() const           { return m_timePreloaded; }
  void                set_time_preloaded(rak::timer t) { m_timePreloaded = t; }

  bool                sync_triggered() const         { return m_asyncTriggered; }
  void                set_sync_triggered(bool v)     { m_asyncTriggered = v; }

  int                 references() const             { return m_references; }
  int                 dec_references()               { return --m_references; }
  int                 inc_references()               { return ++m_references; }

  int                 writable() const               { return m_writable; }
  int                 dec_writable()                 { return --m_writable; }
  int                 inc_writable()                 { return ++m_writable; }

  int                 blocking() const               { return m_blocking; }
  int                 dec_blocking()                 { return --m_blocking; }
  int                 inc_blocking()                 { return ++m_blocking; }

  void                inc_rw()                       { inc_writable(); inc_references(); }
  void                dec_rw()                       { dec_writable(); dec_references(); }

private:
  uint32_t            m_index;
  Chunk*              m_chunk;

  int                 m_references;
  int                 m_writable;
  int                 m_blocking;

  bool                m_asyncTriggered;

  rak::timer          m_timeModified;
  rak::timer          m_timePreloaded;
};

}

#endif

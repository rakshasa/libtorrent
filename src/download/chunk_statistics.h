// libTorrent - BitTorrent library
// Copyright (C) 2005-2007, Jari Sundell
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

#ifndef LIBTORRENT_DOWNLOAD_CHUNK_STATISTICS_H
#define LIBTORRENT_DOWNLOAD_CHUNK_STATISTICS_H

#include <inttypes.h>
#include <vector>

namespace torrent {

class PeerChunks;

class ChunkStatistics : public std::vector<uint8_t> {
public:
  typedef std::vector<uint8_t>            base_type;
  typedef uint32_t                        size_type;

  typedef base_type::value_type           value_type;
  typedef base_type::reference            reference;
  typedef base_type::const_reference      const_reference;
  typedef base_type::iterator             iterator;
  typedef base_type::const_iterator       const_iterator;
  typedef base_type::reverse_iterator     reverse_iterator;

  using base_type::empty;
  using base_type::size;

  static const size_type max_accounted = 255;

  ChunkStatistics() : m_complete(0), m_accounted(0) {}
  ~ChunkStatistics() {}

  size_type           complete() const              { return m_complete; }
  //size_type           incomplete() const;

  // Number of non-complete peers whom's bitfield is added to the
  // statistics.
  size_type           accounted() const             { return m_accounted; }
  
  void                initialize(size_type s);
  void                clear();

  // When a peer connects and sends a non-empty bitfield and is not a
  // seeder, we can be fairly sure it won't just disconnect
  // immediately. Thus it should be resonable to possibly spend the
  // effort adding it to the statistics if nessesary.

  // Where do we decide on policy? On whetever we count the chunks,
  // the type of connection shouldn't matter? As f.ex PCSeed will only
  // make sense when seeding, it won't be counted.

  // Might want to prefer to add peers we are interested in, but which
  // arn't in us.

  void                received_connect(PeerChunks* pc);
  void                received_disconnect(PeerChunks* pc);

  // The caller must ensure that the chunk index is valid and has not
  // been set already.
  void                received_have_chunk(PeerChunks* pc, uint32_t index, uint32_t length);
  
  const_iterator      begin() const                   { return base_type::begin(); }
  const_iterator      end() const                     { return base_type::end(); }

  const_reference     rarity(size_type n) const       { return base_type::operator[](n); }

  const_reference     operator [] (size_type n) const { return base_type::operator[](n); }

private:
  inline bool         should_add(PeerChunks* pc);

  ChunkStatistics(const ChunkStatistics&);
  void operator = (const ChunkStatistics&);

  size_type           m_complete;
  size_type           m_accounted;
};

}

#endif

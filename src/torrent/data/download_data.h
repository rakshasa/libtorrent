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

#ifndef LIBTORRENT_DATA_DOWNLOAD_DATA_H
#define LIBTORRENT_DATA_DOWNLOAD_DATA_H

#include <torrent/common.h>
#include <torrent/bitfield.h>
#include <torrent/utils/ranges.h>

namespace torrent {

class ChunkSelector;
class DownloadWrapper;
class FileList;

class download_data {
public:
  typedef ranges<uint32_t> priority_ranges;

  download_data() : m_wanted_chunks(0) {}

  const Bitfield*        completed_bitfield() const { return &m_completed_bitfield; }
  const Bitfield*        untouched_bitfield() const { return &m_untouched_bitfield; }

  const priority_ranges* high_priority() const      { return &m_high_priority; }
  const priority_ranges* normal_priority() const    { return &m_normal_priority; }

  uint32_t               wanted_chunks() const      { return m_wanted_chunks; }

protected:
  friend class ChunkSelector;
  friend class DownloadWrapper;
  friend class FileList;

  Bitfield*           mutable_completed_bitfield()  { return &m_completed_bitfield; }
  Bitfield*           mutable_untouched_bitfield()  { return &m_untouched_bitfield; }

  priority_ranges*    mutable_high_priority()       { return &m_high_priority; }
  priority_ranges*    mutable_normal_priority()     { return &m_normal_priority; }

  void                set_wanted_chunks(uint32_t n) { m_wanted_chunks = n; }

private:
  Bitfield            m_completed_bitfield;
  Bitfield            m_untouched_bitfield;

  priority_ranges     m_high_priority;
  priority_ranges     m_normal_priority;

  uint32_t            m_wanted_chunks;
};

}

#endif // LIBTORRENT_DATA_DOWNLOAD_DATA_H

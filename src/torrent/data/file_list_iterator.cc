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

#include "config.h"

#include "torrent/exceptions.h"

#include "file.h"
#include "file_list_iterator.h"

namespace torrent {

bool
FileListIterator::is_file() const {
  return m_depth >= 0 && m_depth + 1 == (int32_t)(*m_position)->path()->size();
}

bool
FileListIterator::is_empty() const {
  return (*m_position)->path()->size() == 0;
}

bool
FileListIterator::is_entering() const {
  return m_depth >= 0 && m_depth + 1 != (int32_t)(*m_position)->path()->size();
}

FileListIterator&
FileListIterator::operator ++() {
  int32_t sizePath = (*m_position)->path()->size();

  if (sizePath == 0) {
    m_position++;
    return *this;
  }

  m_depth++;
  
  if (m_depth >= sizePath)
    throw internal_error("FileListIterator::operator ++() m_depth >= sizePath.");

  if (m_depth == sizePath)
    m_depth = -(sizePath - 1);

  if (-m_depth == (int32_t)(*m_position)->match_depth_next()) {
    m_depth = -m_depth;
    m_position++;
  }

  return *this;
}

FileListIterator&
FileListIterator::operator --() {
  // We're guaranteed that if m_depth != 0 then so is the path size,
  // so there's no need to check for it.
  if (m_depth == 0) {
    m_position--;

    // This ensures we properly start iterating the paths in a tree
    // without failing badly when size == 0.
    if ((*m_position)->path()->size() <= 1)
      return *this;

    m_depth--;

    if (m_depth <= -(int32_t)(*m_position)->path()->size())
      throw internal_error("FileListIterator::operator --() m_depth <= -sizePath path 1.");

  } else if (m_depth == (int32_t)(*m_position)->match_depth_prev()) {
    m_position--;
    m_depth = -m_depth;

    if (m_depth <= -(int32_t)(*m_position)->path()->size())
      throw internal_error("FileListIterator::operator --() m_depth <= -sizePath path 2.");

  } else {
    m_depth--;

    if (m_depth <= -(int32_t)(*m_position)->path()->size())
      throw internal_error("FileListIterator::operator --() m_depth <= -sizePath path 3.");
  }

  int32_t sizePath = (*m_position)->path()->size();

  if (m_depth <= -sizePath)
    throw internal_error("FileListIterator::operator --() m_depth <= -sizePath.");

  if (m_depth == -(sizePath - 1))
    m_depth = -m_depth;

  return *this;
}

}

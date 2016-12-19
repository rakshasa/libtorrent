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

#ifndef LIBTORRENT_NET_DATA_BUFFER_H
#define LIBTORRENT_NET_DATA_BUFFER_H

#include <memory>
#include <cinttypes>

namespace torrent {

// Recipient must call clear() when done with the buffer.
struct DataBuffer {
  DataBuffer()                        : m_data(NULL), m_end(NULL), m_owned(true) {}
  DataBuffer(char* data, char* end)   : m_data(data), m_end(end),  m_owned(true) {}

  DataBuffer          clone() const        { DataBuffer d = *this; d.m_owned = false; return d; }
  DataBuffer          release()            { DataBuffer d = *this; set(NULL, NULL, false); return d; }

  char*               data() const         { return m_data; }
  char*               end() const          { return m_end; }

  bool                owned() const        { return m_owned; }
  bool                empty() const        { return m_data == NULL; }
  size_t              length() const       { return m_end - m_data; }

  void                clear();
  void                set(char* data, char* end, bool owned);

private:
  char*               m_data;
  char*               m_end;

  // Used to indicate if buffer held by PCB is its own and needs to be
  // deleted after transmission (false if shared with other connections).
  bool                m_owned;
};

inline void
DataBuffer::clear() {
  if (!empty() && m_owned)
    delete[] m_data;

  m_data = m_end = NULL;
  m_owned = false;
}

inline void
DataBuffer::set(char* data, char* end, bool owned) {
  m_data = data;
  m_end = end;
  m_owned = owned;
}

}

#endif

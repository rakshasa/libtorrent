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

#include "config.h"

#include "exceptions.h"
#include "entry.h"
#include "content/content_file.h"

namespace torrent {

uint64_t
Entry::get_size() {
  return m_entry->get_size();
}

uint32_t
Entry::get_completed() {
  return m_entry->get_completed();
}

uint32_t
Entry::get_chunk_begin() {
  return m_entry->get_range().first;
}

uint32_t
Entry::get_chunk_end() {
  return m_entry->get_range().second;
}  

Entry::Priority
Entry::get_priority() {
  return (Priority)m_entry->get_priority();
}

// Relative to root of torrent.
std::string
Entry::get_path() {
  return m_entry->get_path().path(false);
}

const Entry::Path&
Entry::get_path_list() {
  return m_entry->get_path().list();
}

void
Entry::set_path_list(const Path& l) {
  if (l.empty())
    throw client_error("Tried to set empty path list for Entry");

  m_entry->get_path().list() = l;
}

void
Entry::set_priority(Priority p) {
  m_entry->set_priority(p);
}

}

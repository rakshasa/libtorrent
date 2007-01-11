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

#include <algorithm>

#include "data/socket_file.h"
#include "torrent/exceptions.h"

#include "file.h"
#include "file_manager.h"
#include "manager.h"

namespace torrent {

FileManager::~FileManager() {
  if (!empty())
    throw internal_error("FileManager::~FileManager() called but empty() != true.");
}

void
FileManager::set_max_open_files(size_type s) {
  m_maxOpenFiles = s;

  while (size() > m_maxOpenFiles)
    close_least_active();
}

bool
FileManager::open(value_type file, int prot, int flags) {
  if (file->is_open())
    close(file);

  if (size() > m_maxOpenFiles)
    throw internal_error("FileManager::open_file(...) m_openSize > m_maxOpenFiles.");

  if (size() == m_maxOpenFiles)
    close_least_active();

  SocketFile fd;

  if (!fd.open(file->frozen_path(), prot, flags))
    return false;

  file->set_protection(prot);
  file->set_file_descriptor(fd.fd());
  base_type::push_back(file);

  // Consider storing the position of the file here.

  return true;
}

void
FileManager::close(value_type file) {
  if (!file->is_open())
    return;

  SocketFile(file->file_descriptor()).close();

  file->set_protection(0);
  file->set_file_descriptor(-1);
  
  iterator itr = std::find(begin(), end(), file);

  if (itr == end())
    throw internal_error("FileManager::close_file(...) itr == end().");

  *itr = back();
  base_type::pop_back();
}

struct FileManagerActivity {
  FileManagerActivity() : m_last(rak::timer::max().usec()), m_file(NULL) {}

  void operator ()(File* f) {
    if (f->is_open() && f->last_touched() <= m_last) {
      m_last = f->last_touched();
      m_file = f;
    }
  }

  uint64_t   m_last;
  File*      m_file;
};

void
FileManager::close_least_active() {
  close(std::for_each(begin(), end(), FileManagerActivity()).m_file);
}

}

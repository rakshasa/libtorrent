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
#include <functional>

#include "torrent/exceptions.h"

#include "file_manager.h"
#include "file_meta.h"

namespace torrent {

FileManager::~FileManager() {
  if (!empty())
    throw internal_error("FileManager::~FileManager() called but empty() != true.");
}

void
FileManager::insert(FileMeta* f) {
  f->slot_prepare(rak::make_mem_fun(this, &FileManager::prepare_file));
  base_type::push_back(f);
}

void
FileManager::erase(FileMeta* f) {
  iterator itr = std::find(begin(), end(), f);

  if (itr == end())
    throw internal_error("FileManager::erase(...) could not find FileMeta in container.");

  if (f->is_open())
    close_file(f);

  base_type::erase(itr);
  f->slot_prepare(FileMeta::slot_prepare_type());
}

void
FileManager::set_max_size(size_t s) {
  m_maxSize = s;

  while (m_openSize > m_maxSize)
    close_least_active();
}

bool
FileManager::prepare_file(FileMeta* meta, int prot, int flags) {
  if (meta->is_open())
    close_file(meta);

  if (m_openSize > m_maxSize)
    throw internal_error("FileManager::open_file(...) m_openSize > m_maxSize");

  if (m_openSize == m_maxSize)
    close_least_active();

  if (!meta->get_file().open(meta->get_path(), prot, flags))
    return false;

  ++m_openSize;

  return true;
}

void
FileManager::close_file(FileMeta* meta) {
  if (!meta->is_open())
    throw internal_error("FileManager::close_file(...) called on a closed FileMeta");

  meta->get_file().close();
  --m_openSize;
}

struct FileManagerActivity {
  FileManagerActivity() : m_last(rak::timer::max()), m_meta(NULL) {}

  void operator ()(FileMeta* f) {
    if (f->is_open() && f->get_last_touched() <= m_last) {
      m_last = f->get_last_touched();
      m_meta = f;
    }
  }

  rak::timer m_last;
  FileMeta*  m_meta;
};

void
FileManager::close_least_active() {
  close_file(std::for_each(begin(), end(), FileManagerActivity()).m_meta);
}

}

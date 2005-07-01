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

struct FileMetaRemove {
  void operator() (FileMeta* f) {
    f->get_file().close();
    f->slot_prepare() = FileMeta::SlotPrepare();
    f->slot_disconnect() = FileMeta::SlotDisconnect();
  }
};

void
FileManager::clear() {
  std::for_each(begin(), end(), FileMetaRemove());

  m_openSize = 0;
  Base::clear();
}

void
FileManager::insert(FileMeta* f) {
  if (f->is_valid())
    throw internal_error("FileManager::insert(...) received an already valid FileMeta");

  if (f->is_open())
    if (m_openSize == m_maxSize)
      f->get_file().close();
    else
      ++m_openSize;

  f->slot_prepare() = sigc::mem_fun(*this, &FileManager::prepare_file);
  f->slot_disconnect() = sigc::mem_fun(*this, &FileManager::remove_file);

  // Hmm... insert or push_back?
  Base::push_back(f);
}

void
FileManager::set_max_size(size_t s) {
  m_maxSize = s;

  while (m_openSize > m_maxSize)
    close_least_active();
}

bool
FileManager::prepare_file(FileMeta* meta, int prot) {
  if (meta->is_open())
    close_file(meta);

  if (m_openSize > m_maxSize)
    throw internal_error("FileManager::open_file(...) m_openSize > m_maxSize");

  // Close any files if nessesary.
  if (m_openSize == m_maxSize)
    close_least_active();

  if (!meta->get_file().open(meta->get_path(), prot, 0))
    return false;

  ++m_openSize;

  return true;
}

void
FileManager::remove_file(FileMeta* meta) {
  iterator itr = std::find(begin(), end(), meta);

  if (itr == end())
    throw internal_error("FileManager::close_file(...) could not find FileMeta in container");

  if ((*itr)->is_open())
    close_file(meta);

  // TODO: Use something else here, like swap with last.
  Base::erase(itr);
}

void
FileManager::close_file(FileMeta* meta) {
  if (meta == NULL)
    throw internal_error("FileManager::close_file(...) called on with a NULL argument");

  if (!meta->is_open())
    throw internal_error("FileManager::close_file(...) called on a closed FileMeta");

  meta->get_file().close();
  --m_openSize;
}

struct FileManagerActivity {
  FileManagerActivity() : m_last(Timer::cache()), m_meta(NULL) {}

  void operator ()(FileMeta* f) {
    if (f->is_open() && f->get_last_touched() <= m_last) {
      m_last = f->get_last_touched();
      m_meta = f;
    }
  }

  Timer     m_last;
  FileMeta* m_meta;
};

void
FileManager::close_least_active() {
  close_file(std::for_each(begin(), end(), FileManagerActivity()).m_meta);
}

}

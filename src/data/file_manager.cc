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

#include <algorithm>
#include <functional>

#include "torrent/exceptions.h"

#include "file_manager.h"
#include "file_meta.h"

namespace torrent {

struct FileMetaCloseDelete {
  void operator() (FileMeta* f) {
    f->get_file().close();
    delete f;
  }
};

void
FileManager::clear() {
  std::for_each(begin(), end(), FileMetaCloseDelete());

  m_openSize = 0;
  Base::clear();
}

FileMeta*
FileManager::insert(const std::string& path) {
  return *Base::insert(end(), new FileMeta(path));
}

bool
FileManager::open_file(FileMeta* meta, int flags) {
  if (meta->is_open())
    throw internal_error("FileManager::open_file(...) called on an open iterator");

  if (m_openSize > m_maxSize)
    throw internal_error("FileManager::open_file(...) m_openSize > m_maxSize");

  // Close any files if nessesary.
  if (m_openSize == m_maxSize)
    close_files(1);

  if (!meta->get_file().open(meta->get_path(), flags))
    return false;

  ++m_openSize;

  return true;
}

void
FileManager::close_file(FileMeta* meta) {
  if (!meta->is_open())
    return;

  meta->get_file().close();
  --m_openSize;
}

void
FileManager::close_files(size_t count) {
  if (count > m_openSize)
    throw internal_error("FileManager::close_files(...) count > m_size");

  iterator nth = begin();

  std::advance(nth, count);
  std::nth_element(begin(), nth, end(), std::mem_fun(&FileMeta::comp_less_active));

  if (std::find_if(begin(), nth, std::not1(std::mem_fun(&FileMeta::is_open))) != nth)
    throw internal_error("FileManager::close_files(...) Did not have enough open files");

  std::for_each(begin(), nth, std::bind1st(std::mem_fun(&FileManager::close_file), this));
}

}

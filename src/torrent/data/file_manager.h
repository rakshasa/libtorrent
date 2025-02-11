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

#ifndef LIBTORRENT_DATA_FILE_MANAGER_H
#define LIBTORRENT_DATA_FILE_MANAGER_H

#include <vector>
#include <torrent/common.h>

namespace torrent {

class File;

class LIBTORRENT_EXPORT FileManager : private std::vector<File*> {
public:
  typedef std::vector<File*> base_type;
  typedef uint32_t           size_type;

  using base_type::value_type;
  using base_type::iterator;
  using base_type::reverse_iterator;

  using base_type::begin;
  using base_type::end;
  using base_type::rbegin;
  using base_type::rend;

  FileManager();
  ~FileManager();

  size_type           open_files() const              { return base_type::size(); }

  size_type           max_open_files() const          { return m_maxOpenFiles; }
  void                set_max_open_files(size_type s);

  bool                open(value_type file, int prot, int flags);
  void                close(value_type file);

  void                close_least_active();

  // Statistics:
  uint64_t            files_opened_counter() const { return m_filesOpenedCounter; }
  uint64_t            files_closed_counter() const { return m_filesClosedCounter; }
  uint64_t            files_failed_counter() const { return m_filesFailedCounter; }

private:
  FileManager(const FileManager&) LIBTORRENT_NO_EXPORT;
  void operator = (const FileManager&) LIBTORRENT_NO_EXPORT;

  size_type           m_maxOpenFiles;

  uint64_t            m_filesOpenedCounter;
  uint64_t            m_filesClosedCounter;
  uint64_t            m_filesFailedCounter;
};

}

#endif

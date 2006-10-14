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

#ifndef LIBTORRENT_DOWNLOAD_MANAGER_H
#define LIBTORRENT_DOWNLOAD_MANAGER_H

#include <list>

namespace torrent {

class DownloadWrapper;
class DownloadInfo;
class DownloadMain;

class DownloadManager : private std::list<DownloadWrapper*>{
public:
  typedef std::list<DownloadWrapper*> base_type;

  typedef base_type::value_type value_type;
  typedef base_type::pointer pointer;
  typedef base_type::const_pointer const_pointer;
  typedef base_type::reference reference;
  typedef base_type::const_reference const_reference;
  typedef base_type::size_type size_type;

  typedef base_type::iterator iterator;
  typedef base_type::reverse_iterator reverse_iterator;
  typedef base_type::const_iterator const_iterator;
  typedef base_type::const_reverse_iterator const_reverse_iterator;

  using base_type::empty;
  using base_type::size;

  using base_type::begin;
  using base_type::end;
  using base_type::rbegin;
  using base_type::rend;

  ~DownloadManager() { clear(); }

  iterator            insert(DownloadWrapper* d);
  iterator            erase(DownloadWrapper* d);

  void                clear();

  iterator            find(const std::string& hash);
  iterator            find(DownloadInfo* info);
  DownloadMain*       find_main(const std::string& hash);
  DownloadMain*       find_main_obfuscated(const std::string& hash);
};

}

#endif

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

#ifndef LIBTORRENT_DOWNLOAD_MANAGER_H
#define LIBTORRENT_DOWNLOAD_MANAGER_H

#include <list>

namespace torrent {

class DownloadWrapper;

class DownloadManager : private std::list<DownloadWrapper*>{
public:
  typedef std::list<DownloadWrapper*> Base;

  typedef Base::value_type value_type;
  typedef Base::pointer pointer;
  typedef Base::const_pointer const_pointer;
  typedef Base::reference reference;
  typedef Base::const_reference const_reference;
  typedef Base::size_type size_type;

  typedef Base::iterator iterator;
  typedef Base::reverse_iterator reverse_iterator;
  typedef Base::const_iterator const_iterator;
  typedef Base::const_reverse_iterator const_reverse_iterator;

  using Base::empty;
  using Base::size;

  using Base::begin;
  using Base::end;
  using Base::rbegin;
  using Base::rend;

  ~DownloadManager() { clear(); }

  void                 add(DownloadWrapper* d);
  void                 remove(const std::string& hash);

  void                 clear();

  DownloadWrapper*     find(const std::string& hash);
};

}

#endif

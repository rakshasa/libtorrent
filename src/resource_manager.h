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

#ifndef LIBTORRENT_DOWNLOAD_RESOURCE_MANAGER_H
#define LIBTORRENT_DOWNLOAD_RESOURCE_MANAGER_H

#include <vector>

namespace torrent {

// This class will handle the division of various resources like
// uploads. For now the weight is equal to the value of the priority.
//
// Add unlimited handling later.

class DownloadMain;

class ResourceManager : public std::vector<std::pair<int, DownloadMain*> > {
public:
  typedef std::vector<std::pair<int, DownloadMain*> > Base;
  typedef Base::value_type                            value_type;

  using Base::begin;
  using Base::end;

  ResourceManager() :
    m_maxUnchoked(0),
    m_currentlyUnchoked(0) {}
  ~ResourceManager();

  void                insert(int priority, DownloadMain* d);
  void                erase(DownloadMain* d);

  unsigned int        max_unchoked() const             { return m_maxUnchoked; }
  void                set_max_unchoked(unsigned int m) { m_maxUnchoked = m; }

  void                receive_choke();
  bool                receive_unchoke();

  void                receive_tick();

private:
  unsigned int        total_weight() const;
  void                balance_unchoked(unsigned int weight);

  unsigned int        m_currentlyUnchoked;
  unsigned int        m_maxUnchoked;
};

}

#endif

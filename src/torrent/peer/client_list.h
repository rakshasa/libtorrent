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

#ifndef LIBTORRENT_PEER_CLIENT_LIST_H
#define LIBTORRENT_PEER_CLIENT_LIST_H

#include <vector>
#include <torrent/peer/client_info.h>

namespace torrent {

class LIBTORRENT_EXPORT ClientList : private std::vector<ClientInfo> {
public:
  typedef std::vector<ClientInfo> base_type;
  typedef uint32_t                size_type;

  using base_type::value_type;
  using base_type::reference;
  using base_type::difference_type;

  using base_type::iterator;
  using base_type::reverse_iterator;
  using base_type::size;
  using base_type::empty;

  using base_type::begin;
  using base_type::end;
  using base_type::rbegin;
  using base_type::rend;
  
  ClientList();

  iterator            insert(ClientInfo::id_type type, const char* key, const char* version, const char* upperVersion);

  // Helper functions which only require the key to be as long as the
  // key for that specific id type.
  iterator            insert_helper(ClientInfo::id_type type,
                                    const char* key,
                                    const char* version,
                                    const char* upperVersion,
                                    const char* shortDescription);

  bool                retrieve_id(ClientInfo* dest, const HashString& id) const;
  void                retrieve_unknown(ClientInfo* dest) const;
};

}

#endif

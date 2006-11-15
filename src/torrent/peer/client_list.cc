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
#include <rak/functional.h>
#include <rak/string_manip.h>

#include "client_list.h"
#include "exceptions.h"
#include "hash_string.h"

namespace torrent {

ClientList::ClientList() {
  insert(ClientInfo::TYPE_UNKNOWN, NULL, NULL, NULL);

  // Move this to a seperate initialize function in libtorrent.
  iterator itr = insert(ClientInfo::TYPE_AZUREUS, "AZ", NULL, NULL);
  itr->set_short_description("Azureus");
}

ClientList::iterator
ClientList::insert(ClientInfo::id_type type, const char* key, const char* version, const char* upperVersion) {
  if (type >= ClientInfo::TYPE_MAX_SIZE)
    throw input_error("Invalid client info id type.");

  ClientInfo clientInfo;

  clientInfo.set_type(type);
  clientInfo.set_info(new ClientInfo::info_type);
  clientInfo.set_short_description("Unknown");

  std::memset(clientInfo.mutable_key(), 0, ClientInfo::max_key_size);

  if (key == NULL)
    std::memset(clientInfo.mutable_key(), 0, ClientInfo::max_key_size);
  else
    std::strncpy(clientInfo.mutable_key(), key, ClientInfo::max_key_size);
    
  if (version != NULL)
    std::memcpy(clientInfo.mutable_version(), version, ClientInfo::max_version_size);
  else
    std::memset(clientInfo.mutable_version(), 0, ClientInfo::max_version_size);

  if (upperVersion != NULL)
    std::memcpy(clientInfo.mutable_upper_version(), upperVersion, ClientInfo::max_version_size);
  else
    std::memset(clientInfo.mutable_upper_version(), -1, ClientInfo::max_version_size);

  return base_type::insert(end(), clientInfo);
}

// Make this properly honor const-ness.
ClientInfo
ClientList::parse_id(const HashString& id) const {
  ClientInfo clientInfo;

  if (id[0] == '-' && id[7] == '-' &&
      std::isalpha(id[1]) && std::isalpha(id[2]) &&
      std::isxdigit(id[3]) && std::isxdigit(id[4]) && std::isxdigit(id[5]) && std::isxdigit(id[6])) {
    clientInfo.set_type(ClientInfo::TYPE_AZUREUS);

    clientInfo.mutable_key()[0] = id[1];
    clientInfo.mutable_key()[1] = id[2];
    
    for (int i = 0; i < 4; i++)
      clientInfo.mutable_version()[i] = clientInfo.mutable_upper_version()[i] = rak::hexchar_to_value(id[3 + i]);

    // And then the incompatible idiots that make life difficult for us
    // others. (There's '3' schemes to choose from already...)
    //
    // Or not...

  } else {
    // The first entry always contains the default ClientInfo.
    return *begin();
  }

  const_iterator itr = std::find_if(begin() + 1, end(), rak::bind1st(std::ptr_fun(&ClientInfo::intersects), clientInfo));

  if (itr == end())
    clientInfo.set_info(begin()->info());
  else
    clientInfo.set_info(itr->info());    

  return clientInfo;
}

}

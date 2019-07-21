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

#include "config.h"

#include <algorithm>
#include <rak/string_manip.h>
#include lt_tr1_functional

#include "client_list.h"
#include "exceptions.h"
#include "hash_string.h"

namespace torrent {

ClientList::ClientList() {
  insert(ClientInfo::TYPE_UNKNOWN, nullptr, nullptr, nullptr);

  // Move this to a seperate initialize function in libtorrent.

  // Sorted by popularity to optimize search. This list is heavily
  // biased by my own prejudices, and not at all based on facts.

  // First batch of clients.
  insert_helper(ClientInfo::TYPE_AZUREUS, "AZ", nullptr, nullptr, "Azureus");
  insert_helper(ClientInfo::TYPE_AZUREUS, "BC", nullptr, nullptr, "BitComet");
  insert_helper(ClientInfo::TYPE_AZUREUS, "CD", nullptr, nullptr, "Enhanced CTorrent");
  insert_helper(ClientInfo::TYPE_AZUREUS, "KT", nullptr, nullptr, "KTorrent");
  insert_helper(ClientInfo::TYPE_AZUREUS, "LT", nullptr, nullptr, "libtorrent");
  insert_helper(ClientInfo::TYPE_AZUREUS, "lt", nullptr, nullptr, "libTorrent");
  insert_helper(ClientInfo::TYPE_AZUREUS, "UM", nullptr, nullptr, "uTorrent Mac");
  insert_helper(ClientInfo::TYPE_AZUREUS, "UT", nullptr, nullptr, "uTorrent");

  insert_helper(ClientInfo::TYPE_MAINLINE, "M", nullptr, nullptr, "Mainline");

  insert_helper(ClientInfo::TYPE_COMPACT, "T", nullptr, nullptr, "BitTornado");

  // Second batch of clients.
  insert_helper(ClientInfo::TYPE_AZUREUS, "AR", nullptr, nullptr, "Arctic");
  insert_helper(ClientInfo::TYPE_AZUREUS, "BB", nullptr, nullptr, "BitBuddy");
  insert_helper(ClientInfo::TYPE_AZUREUS, "BX", nullptr, nullptr, "Bittorrent X");
  insert_helper(ClientInfo::TYPE_AZUREUS, "BS", nullptr, nullptr, "BTSlave");
  insert_helper(ClientInfo::TYPE_AZUREUS, "BT", nullptr, nullptr, "BBTor");
  insert_helper(ClientInfo::TYPE_AZUREUS, "CT", nullptr, nullptr, "CTorrent");
  insert_helper(ClientInfo::TYPE_AZUREUS, "DE", nullptr, nullptr, "DelugeTorrent");
  insert_helper(ClientInfo::TYPE_AZUREUS, "ES", nullptr, nullptr, "Electric Sheep");
  insert_helper(ClientInfo::TYPE_AZUREUS, "LP", nullptr, nullptr, "Lphant");
  insert_helper(ClientInfo::TYPE_AZUREUS, "MT", nullptr, nullptr, "MoonlightTorrent");
  insert_helper(ClientInfo::TYPE_AZUREUS, "MP", nullptr, nullptr, "MooPolice");
  insert_helper(ClientInfo::TYPE_AZUREUS, "QT", nullptr, nullptr, "Qt 4 Torrent");
  insert_helper(ClientInfo::TYPE_AZUREUS, "RT", nullptr, nullptr, "Retriever");
  insert_helper(ClientInfo::TYPE_AZUREUS, "SZ", nullptr, nullptr, "Shareaza");
  insert_helper(ClientInfo::TYPE_AZUREUS, "SS", nullptr, nullptr, "SwarmScope");
  insert_helper(ClientInfo::TYPE_AZUREUS, "SB", nullptr, nullptr, "Swiftbit");
  insert_helper(ClientInfo::TYPE_AZUREUS, "TN", nullptr, nullptr, "TorrentDotNET");
  insert_helper(ClientInfo::TYPE_AZUREUS, "TS", nullptr, nullptr, "Torrentstorm");
  insert_helper(ClientInfo::TYPE_AZUREUS, "TR", nullptr, nullptr, "Transmission");
  insert_helper(ClientInfo::TYPE_AZUREUS, "XT", nullptr, nullptr, "XanTorrent");
  insert_helper(ClientInfo::TYPE_AZUREUS, "ZT", nullptr, nullptr, "ZipTorrent");

  insert_helper(ClientInfo::TYPE_COMPACT, "A", nullptr, nullptr, "ABC");
  insert_helper(ClientInfo::TYPE_COMPACT, "S", nullptr, nullptr, "Shadow's client");
  insert_helper(ClientInfo::TYPE_COMPACT, "U", nullptr, nullptr, "UPnP NAT BitTorrent");
  insert_helper(ClientInfo::TYPE_COMPACT, "O", nullptr, nullptr, "Osprey Permaseed");

  // Third batch of clients.
  insert_helper(ClientInfo::TYPE_AZUREUS, "AX", nullptr, nullptr, "BitPump");
  insert_helper(ClientInfo::TYPE_AZUREUS, "BF", nullptr, nullptr, "BitFlu");
  insert_helper(ClientInfo::TYPE_AZUREUS, "BG", nullptr, nullptr, "BTG");
  insert_helper(ClientInfo::TYPE_AZUREUS, "BR", nullptr, nullptr, "BitRocket");
  insert_helper(ClientInfo::TYPE_AZUREUS, "EB", nullptr, nullptr, "EBit");
  insert_helper(ClientInfo::TYPE_AZUREUS, "HL", nullptr, nullptr, "Halite");
  insert_helper(ClientInfo::TYPE_AZUREUS, "qB", nullptr, nullptr, "qBittorrent");
  insert_helper(ClientInfo::TYPE_AZUREUS, "UL", nullptr, nullptr, "uLeecher!");
  insert_helper(ClientInfo::TYPE_AZUREUS, "XL", nullptr, nullptr, "XeiLun");

  insert_helper(ClientInfo::TYPE_COMPACT, "R", nullptr, nullptr, "Tribler");
}

ClientList::~ClientList() {
  for (iterator itr = begin(), last = end(); itr != last; ++itr)
    delete itr->info();
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

  if (key == nullptr)
    std::memset(clientInfo.mutable_key(), 0, ClientInfo::max_key_size);
  else
    std::strncpy(clientInfo.mutable_key(), key, ClientInfo::max_key_size);
    
  if (version != nullptr)
    std::memcpy(clientInfo.mutable_version(), version, ClientInfo::max_version_size);
  else
    std::memset(clientInfo.mutable_version(), 0, ClientInfo::max_version_size);

  if (upperVersion != nullptr)
    std::memcpy(clientInfo.mutable_upper_version(), upperVersion, ClientInfo::max_version_size);
  else
    std::memset(clientInfo.mutable_upper_version(), -1, ClientInfo::max_version_size);

  return base_type::insert(end(), clientInfo);
}

ClientList::iterator
ClientList::insert_helper(ClientInfo::id_type type,
                          const char* key,
                          const char* version,
                          const char* upperVersion,
                          const char* shortDescription) {
  char newKey[ClientInfo::max_key_size];

  std::memset(newKey, 0, ClientInfo::max_key_size);
  std::memcpy(newKey, key, ClientInfo::key_size(type));

  iterator itr = insert(type, newKey, version, upperVersion);
  itr->set_short_description(shortDescription);

  return itr;
}

// Make this properly honor const-ness.
bool
ClientList::retrieve_id(ClientInfo* dest, const HashString& id) const {
  if (id[0] == '-' && id[7] == '-' &&
      std::isalpha(id[1]) && std::isalpha(id[2]) &&
      std::isxdigit(id[3]) && std::isxdigit(id[4]) && std::isxdigit(id[5]) && std::isxdigit(id[6])) {
    dest->set_type(ClientInfo::TYPE_AZUREUS);

    dest->mutable_key()[0] = id[1];
    dest->mutable_key()[1] = id[2];
    
    for (int i = 0; i < 4; i++)
      dest->mutable_version()[i] = dest->mutable_upper_version()[i] = rak::hexchar_to_value(id[3 + i]);

  } else if (std::isalpha(id[0]) && id[4] == '-' &&
             std::isxdigit(id[1]) && std::isxdigit(id[2]) && std::isxdigit(id[3])) {
    dest->set_type(ClientInfo::TYPE_COMPACT);

    dest->mutable_key()[0] = id[0];
    dest->mutable_key()[1] = '\0';
    
    dest->mutable_version()[0] = dest->mutable_upper_version()[0] = rak::hexchar_to_value(id[1]);
    dest->mutable_version()[1] = dest->mutable_upper_version()[1] = rak::hexchar_to_value(id[2]);
    dest->mutable_version()[2] = dest->mutable_upper_version()[2] = rak::hexchar_to_value(id[3]);
    dest->mutable_version()[3] = dest->mutable_upper_version()[3] = '\0';

  } else if (std::isalpha(id[0]) && std::isdigit(id[1]) && id[2] == '-' &&
             std::isdigit(id[3]) && (id[6] == '-' || id[7] == '-')) {

    dest->set_type(ClientInfo::TYPE_MAINLINE);

    dest->mutable_key()[0] = id[0];
    dest->mutable_key()[1] = '\0';
    
    dest->mutable_version()[0] = dest->mutable_upper_version()[0] = rak::hexchar_to_value(id[1]);

    if (id[4] == '-' && std::isdigit(id[5]) && id[6] == '-') {
      dest->mutable_version()[1] = dest->mutable_upper_version()[1] = rak::hexchar_to_value(id[3]);
      dest->mutable_version()[2] = dest->mutable_upper_version()[2] = rak::hexchar_to_value(id[5]);
      dest->mutable_version()[3] = dest->mutable_upper_version()[3] = '\0';

    } else if (std::isdigit(id[4]) && id[5] == '-' && std::isdigit(id[6]) && id[7] == '-') {
      dest->mutable_version()[1] = dest->mutable_upper_version()[1] = rak::hexchar_to_value(id[3]) * 10 + rak::hexchar_to_value(id[4]);
      dest->mutable_version()[2] = dest->mutable_upper_version()[2] = rak::hexchar_to_value(id[6]);
      dest->mutable_version()[3] = dest->mutable_upper_version()[3] = '\0';

    } else {
      *dest = *begin();
      std::memset(dest->mutable_upper_version(), 0, ClientInfo::max_version_size);

      return false;
    }

  } else {
    // And then the incompatible idiots that make life difficult for us
    // others. (There's '3' schemes to choose from already...)
    //
    // Or not...

    // The first entry always contains the default ClientInfo.
    *dest = *begin();
    std::memset(dest->mutable_upper_version(), 0, ClientInfo::max_version_size);

    return false;
  }

  const_iterator itr = std::find_if(begin() + 1, end(), std::bind(&ClientInfo::intersects, *dest, std::placeholders::_1));

  if (itr == end())
    dest->set_info(begin()->info());
  else
    dest->set_info(itr->info());    

  return true;
}

void
ClientList::retrieve_unknown(ClientInfo* dest) const {
  *dest = *begin();
  std::memset(dest->mutable_upper_version(), 0, ClientInfo::max_version_size);
}

}

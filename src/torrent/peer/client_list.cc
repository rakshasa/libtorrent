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

#include "client_list.h"
#include "exceptions.h"
#include "hash_string.h"

namespace torrent {

ClientList::ClientList() {
  insert(ClientInfo::TYPE_UNKNOWN, NULL, NULL, NULL);

  // Move this to a seperate initialize function in libtorrent.

  // Sorted by popularity to optimize search. This list is heavily
  // biased by my own prejudices, and not at all based on facts.

  // First batch of clients.
  // Updated list of clients.
  insert_helper(ClientInfo::TYPE_AZUREUS, "lt", NULL, NULL, "libTorrent");
  insert_helper(ClientInfo::TYPE_AZUREUS, "qB", NULL, NULL, "qBittorrent");
  insert_helper(ClientInfo::TYPE_AZUREUS, "UT", NULL, NULL, "uTorrent");
  insert_helper(ClientInfo::TYPE_AZUREUS, "TR", NULL, NULL, "Transmission");
  insert_helper(ClientInfo::TYPE_AZUREUS, "DE", NULL, NULL, "DelugeTorrent");
  insert_helper(ClientInfo::TYPE_AZUREUS, "AZ", NULL, NULL, "Vuze");
  insert_helper(ClientInfo::TYPE_AZUREUS, "UM", NULL, NULL, "uTorrent Mac");
  insert_helper(ClientInfo::TYPE_AZUREUS, "LT", NULL, NULL, "libtorrent");
  insert_helper(ClientInfo::TYPE_AZUREUS, "BT", NULL, NULL, "Mainline");
  insert_helper(ClientInfo::TYPE_MAINLINE, "M", NULL, NULL, "Mainline");
  insert_helper(ClientInfo::TYPE_AZUREUS, "A2", NULL, NULL, "aria2");
  insert_helper(ClientInfo::TYPE_AZUREUS, "BC", NULL, NULL, "BitComet");
  insert_helper(ClientInfo::TYPE_AZUREUS, "XL", NULL, NULL, "Xunlei");
  insert_helper(ClientInfo::TYPE_AZUREUS, "SD", NULL, NULL, "Xunlei");

  // Other clients.
  insert_helper(ClientInfo::TYPE_AZUREUS, "7T", NULL, NULL, "aTorrent");
  insert_helper(ClientInfo::TYPE_COMPACT, "A", NULL, NULL, "ABC");
  insert_helper(ClientInfo::TYPE_AZUREUS, "A~", NULL, NULL, "Ares");
  insert_helper(ClientInfo::TYPE_AZUREUS, "AG", NULL, NULL, "Ares");
  insert_helper(ClientInfo::TYPE_AZUREUS, "AN", NULL, NULL, "Ares");
  insert_helper(ClientInfo::TYPE_AZUREUS, "AR", NULL, NULL, "Ares"); // Ares is more likely than ArcticTorrent
  insert_helper(ClientInfo::TYPE_AZUREUS, "AT", NULL, NULL, "Artemis");
  insert_helper(ClientInfo::TYPE_AZUREUS, "AV", NULL, NULL, "Avicora");
  insert_helper(ClientInfo::TYPE_AZUREUS, "AX", NULL, NULL, "BitPump");
  insert_helper(ClientInfo::TYPE_AZUREUS, "BB", NULL, NULL, "BitBuddy");
  insert_helper(ClientInfo::TYPE_AZUREUS, "BE", NULL, NULL, "BitTorrent SDK");
  insert_helper(ClientInfo::TYPE_AZUREUS, "BF", NULL, NULL, "BitFlu");
  insert_helper(ClientInfo::TYPE_AZUREUS, "BG", NULL, NULL, "BTGetit");
  insert_helper(ClientInfo::TYPE_AZUREUS, "BI", NULL, NULL, "BiglyBT");
  insert_helper(ClientInfo::TYPE_AZUREUS, "bk", NULL, NULL, "BitKitten (libtorrent)");
  insert_helper(ClientInfo::TYPE_AZUREUS, "BM", NULL, NULL, "BitMagnet");
  insert_helper(ClientInfo::TYPE_AZUREUS, "BP", NULL, NULL, "BitTorrent Pro");
  insert_helper(ClientInfo::TYPE_AZUREUS, "BR", NULL, NULL, "BitRocket");
  insert_helper(ClientInfo::TYPE_AZUREUS, "BS", NULL, NULL, "BTSlave");
  insert_helper(ClientInfo::TYPE_AZUREUS, "BW", NULL, NULL, "BitWombat");
  insert_helper(ClientInfo::TYPE_AZUREUS, "BX", NULL, NULL, "Bittorrent X");
  insert_helper(ClientInfo::TYPE_AZUREUS, "CB", NULL, NULL, "Shareaza Plus");
  insert_helper(ClientInfo::TYPE_AZUREUS, "CD", NULL, NULL, "Enhanced CTorrent");
  insert_helper(ClientInfo::TYPE_AZUREUS, "cT", NULL, NULL, "CuteTorrent");
  insert_helper(ClientInfo::TYPE_AZUREUS, "CT", NULL, NULL, "CTorrent");
  insert_helper(ClientInfo::TYPE_AZUREUS, "DP", NULL, NULL, "Propogate Data Client");
  insert_helper(ClientInfo::TYPE_AZUREUS, "EB", NULL, NULL, "EBit");
  insert_helper(ClientInfo::TYPE_AZUREUS, "ES", NULL, NULL, "Electric Sheep");
  insert_helper(ClientInfo::TYPE_AZUREUS, "FC", NULL, NULL, "FileCroc");
  insert_helper(ClientInfo::TYPE_AZUREUS, "FD", NULL, NULL, "Free Download Manager");
  insert_helper(ClientInfo::TYPE_AZUREUS, "FG", NULL, NULL, "FlashGet");
  insert_helper(ClientInfo::TYPE_AZUREUS, "FL", NULL, NULL, "Flud");
  insert_helper(ClientInfo::TYPE_AZUREUS, "FT", NULL, NULL, "FoxTorrent/RedSwoosh");
  insert_helper(ClientInfo::TYPE_AZUREUS, "FW", NULL, NULL, "FrostWire");
  insert_helper(ClientInfo::TYPE_AZUREUS, "FX", NULL, NULL, "Freebox BitTorrent");
  insert_helper(ClientInfo::TYPE_AZUREUS, "GR", NULL, NULL, "GetRight");
  insert_helper(ClientInfo::TYPE_AZUREUS, "GS", NULL, NULL, "GSTorrent");
  insert_helper(ClientInfo::TYPE_AZUREUS, "GT", NULL, NULL, "go.torrent");
  insert_helper(ClientInfo::TYPE_AZUREUS, "HL", NULL, NULL, "Halite");
  insert_helper(ClientInfo::TYPE_AZUREUS, "HN", NULL, NULL, "Hydranode");
  insert_helper(ClientInfo::TYPE_AZUREUS, "IL", NULL, NULL, "iLivid");
  insert_helper(ClientInfo::TYPE_AZUREUS, "JS", NULL, NULL, "JSTorrent");
  insert_helper(ClientInfo::TYPE_AZUREUS, "JT", NULL, NULL, "jTorrent");
  insert_helper(ClientInfo::TYPE_AZUREUS, "jT", NULL, NULL, "jTorrent");
  insert_helper(ClientInfo::TYPE_AZUREUS, "KG", NULL, NULL, "KGet");
  insert_helper(ClientInfo::TYPE_AZUREUS, "KT", NULL, NULL, "KTorrent");
  insert_helper(ClientInfo::TYPE_AZUREUS, "LC", NULL, NULL, "LeechCraft");
  insert_helper(ClientInfo::TYPE_AZUREUS, "LH", NULL, NULL, "LH-ABC");
  insert_helper(ClientInfo::TYPE_AZUREUS, "LK", NULL, NULL, "linkage");
  insert_helper(ClientInfo::TYPE_AZUREUS, "LP", NULL, NULL, "Lphant");
  insert_helper(ClientInfo::TYPE_AZUREUS, "Lr", NULL, NULL, "LibreTorrent");
  insert_helper(ClientInfo::TYPE_AZUREUS, "LW", NULL, NULL, "LimeWire");
  insert_helper(ClientInfo::TYPE_AZUREUS, "MG", NULL, NULL, "MediaGet");
  insert_helper(ClientInfo::TYPE_AZUREUS, "MO", NULL, NULL, "MonoTorrent");
  insert_helper(ClientInfo::TYPE_AZUREUS, "MP", NULL, NULL, "MooPolice");
  insert_helper(ClientInfo::TYPE_AZUREUS, "MR", NULL, NULL, "Miro");
  insert_helper(ClientInfo::TYPE_AZUREUS, "MT", NULL, NULL, "MoonlightTorrent");
  insert_helper(ClientInfo::TYPE_AZUREUS, "NE", NULL, NULL, "BT Next Evolution");
  insert_helper(ClientInfo::TYPE_AZUREUS, "NX", NULL, NULL, "Net Transport");
  insert_helper(ClientInfo::TYPE_COMPACT, "O", NULL, NULL, "Osprey Permaseed");
  insert_helper(ClientInfo::TYPE_AZUREUS, "OS", NULL, NULL, "OneSwarm");
  insert_helper(ClientInfo::TYPE_AZUREUS, "OT", NULL, NULL, "OmegaTorrent");
  insert_helper(ClientInfo::TYPE_AZUREUS, "PC", NULL, NULL, "CacheLogic");
  insert_helper(ClientInfo::TYPE_AZUREUS, "PI", NULL, NULL, "PicoTorrent");
  insert_helper(ClientInfo::TYPE_AZUREUS, "PT", NULL, NULL, "Popcorn Time");
  insert_helper(ClientInfo::TYPE_AZUREUS, "PD", NULL, NULL, "Pando");
  insert_helper(ClientInfo::TYPE_AZUREUS, "pX", NULL, NULL, "pHoeniX");
  insert_helper(ClientInfo::TYPE_COMPACT, "Q", NULL, NULL, "BTQueue");
  insert_helper(ClientInfo::TYPE_AZUREUS, "QD", NULL, NULL, "qqdownload");
  insert_helper(ClientInfo::TYPE_AZUREUS, "QT", NULL, NULL, "Qt 4 Torrent");
  insert_helper(ClientInfo::TYPE_COMPACT, "R", NULL, NULL, "Tribler");
  insert_helper(ClientInfo::TYPE_AZUREUS, "RS", NULL, NULL, "Rufus");
  insert_helper(ClientInfo::TYPE_AZUREUS, "RT", NULL, NULL, "Retriever");
  insert_helper(ClientInfo::TYPE_AZUREUS, "RZ", NULL, NULL, "RezTorrent");
  insert_helper(ClientInfo::TYPE_COMPACT, "S", NULL, NULL, "Shadow's client");
  insert_helper(ClientInfo::TYPE_AZUREUS, "S~", NULL, NULL, "Shareaza alpha/beta");
  insert_helper(ClientInfo::TYPE_AZUREUS, "SB", NULL, NULL, "SwiftBit");
  insert_helper(ClientInfo::TYPE_AZUREUS, "SG", NULL, NULL, "GS Torrent");
  insert_helper(ClientInfo::TYPE_AZUREUS, "SK", NULL, NULL, "Spark");
  insert_helper(ClientInfo::TYPE_AZUREUS, "SM", NULL, NULL, "SoMud");
  insert_helper(ClientInfo::TYPE_AZUREUS, "SN", NULL, NULL, "ShareNET");
  insert_helper(ClientInfo::TYPE_AZUREUS, "SP", NULL, NULL, "BitSpirit");
  insert_helper(ClientInfo::TYPE_AZUREUS, "SS", NULL, NULL, "SwarmScope");
  insert_helper(ClientInfo::TYPE_AZUREUS, "ST", NULL, NULL, "SymTorrent");
  insert_helper(ClientInfo::TYPE_AZUREUS, "st", NULL, NULL, "SharkTorrent");
  insert_helper(ClientInfo::TYPE_AZUREUS, "SZ", NULL, NULL, "Shareaza");
  insert_helper(ClientInfo::TYPE_AZUREUS, "tT", NULL, NULL, "tTorrent");
  insert_helper(ClientInfo::TYPE_COMPACT, "T", NULL, NULL, "BitTornado");
  insert_helper(ClientInfo::TYPE_AZUREUS, "TB", NULL, NULL, "Torch");
  insert_helper(ClientInfo::TYPE_AZUREUS, "TG", NULL, NULL, "Torrent GO");
  insert_helper(ClientInfo::TYPE_AZUREUS, "TL", NULL, NULL, "Tribler");
  insert_helper(ClientInfo::TYPE_AZUREUS, "TN", NULL, NULL, "Torrent.NET");
  insert_helper(ClientInfo::TYPE_AZUREUS, "TS", NULL, NULL, "Torrentstorm");
  insert_helper(ClientInfo::TYPE_AZUREUS, "TT", NULL, NULL, "TuoTu");
  insert_helper(ClientInfo::TYPE_COMPACT, "U", NULL, NULL, "UPnP NAT BitTorrent");
  insert_helper(ClientInfo::TYPE_AZUREUS, "UE", NULL, NULL, "uTorrent Embedded");
  insert_helper(ClientInfo::TYPE_AZUREUS, "UL", NULL, NULL, "uLeecher!");
  insert_helper(ClientInfo::TYPE_AZUREUS, "UW", NULL, NULL, "uTorrent Web");
  insert_helper(ClientInfo::TYPE_AZUREUS, "WD", NULL, NULL, "WebTorrent Desktop");
  insert_helper(ClientInfo::TYPE_AZUREUS, "WT", NULL, NULL, "Bitlet");
  insert_helper(ClientInfo::TYPE_AZUREUS, "WW", NULL, NULL, "WebTorrent");
  insert_helper(ClientInfo::TYPE_AZUREUS, "WY", NULL, NULL, "FireTorrent");
  insert_helper(ClientInfo::TYPE_AZUREUS, "VG", NULL, NULL, "Vagaa");
  insert_helper(ClientInfo::TYPE_AZUREUS, "XC", NULL, NULL, "XTorrent");
  insert_helper(ClientInfo::TYPE_AZUREUS, "XF", NULL, NULL, "Xfplay");
  insert_helper(ClientInfo::TYPE_AZUREUS, "XT", NULL, NULL, "XanTorrent");
  insert_helper(ClientInfo::TYPE_AZUREUS, "XX", NULL, NULL, "XTorrent");
  insert_helper(ClientInfo::TYPE_AZUREUS, "ZO", NULL, NULL, "Zona");
  insert_helper(ClientInfo::TYPE_AZUREUS, "ZT", NULL, NULL, "ZipTorrent");
} 

ClientList::iterator
ClientList::insert(ClientInfo::id_type type, const char* key, const char* version, const char* upperVersion) {
  if (type >= ClientInfo::TYPE_MAX_SIZE)
    throw input_error("Invalid client info id type.");

  ClientInfo clientInfo;

  clientInfo.set_type(type);
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

ClientList::iterator
ClientList::insert_helper(ClientInfo::id_type type,
                          const char* key,
                          const char* version,
                          const char* upperVersion,
                          const char* shortDescription) {
  char newKey[ClientInfo::max_key_size];

  std::memset(newKey, 0, ClientInfo::max_key_size);
  std::memcpy(newKey, key, ClientInfo::key_size(type));

  auto itr = insert(type, newKey, version, upperVersion);
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

  auto itr = std::find_if(std::next(begin()), end(), [dest](const auto& client) {
    return ClientInfo::intersects(*dest, client);
  });
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

} // namespace torrent

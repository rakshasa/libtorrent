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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <inttypes.h>

#include "general.h"
#include "utils/bitfield.h"
#include "torrent/exceptions.h"
#include "settings.h"
#include "torrent/bencode.h"

#include <stdlib.h>
#include <iomanip>
#include <sstream>
#include <openssl/sha.h>
#include <sys/time.h>

namespace torrent {

std::string generateId() {
  std::string id = Settings::peerName;

  for (int i = id.length(); i < 20; ++i)
    id += random();

  return id;
}

std::string generateKey() {
  std::string id;

  for (int i = 0; i < 8; ++i) {
    unsigned int v = random() % 16;

    if (v < 10)
      id += '0' + v;
    else
      id += 'a' + v - 10;
  }

  return id;
}

std::string calcHash(const Bencode& b) {
  std::stringstream str;
  str << b;

  return std::string((const char*)SHA1((const unsigned char*)(str.str().c_str()), str.str().length(), NULL), 20);
}

}

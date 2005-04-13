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

#ifndef LIBTORRENT_HASH_COMPUTE_H
#define LIBTORRENT_HASH_COMPUTE_H

#include <string>
#include "sha_fast.h"

namespace torrent {

class Sha1 {
public:
  void        init()                      { SHA1_Begin(&m_ctx); }

  void        update(const void* data,
		     unsigned int length) { SHA1_Update(&m_ctx, (unsigned char*)data, length); }

  std::string final() {
    unsigned int len;
    unsigned char buf[20];

    SHA1_End(&m_ctx, buf, &len, 20);
    
    return std::string((char*)buf, 20);
  }

private:
  SHA1Context m_ctx;
};

}

#endif

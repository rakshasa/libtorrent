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

#ifndef LIBTORRENT_HASH_COMPUTE_H
#define LIBTORRENT_HASH_COMPUTE_H

#include <string>

#if defined USE_NSS_SHA
#include "sha_fast.h"
#elif defined USE_OPENSSL_SHA
#include <openssl/sha.h>
#else
#error "No SHA1 implementation selected, choose between Mozilla's NSS and OpenSSL."
#endif

namespace torrent {

class Sha1 {
public:
  void                init();
  void                update(const void* data, unsigned int length);
  std::string         final();

#if defined USE_NSS_SHA

private:
  SHA1Context m_ctx;
};

inline void
Sha1::init() {
  SHA1_Begin(&m_ctx);
}

inline void
Sha1::update(const void* data, unsigned int length) {
  SHA1_Update(&m_ctx, (unsigned char*)data, length);
}

inline std::string
Sha1::final() {
  unsigned int len;
  unsigned char buf[20];
  
  SHA1_End(&m_ctx, buf, &len, 20);
  
  return std::string((char*)buf, 20);
}

#elif defined USE_OPENSSL_SHA

private:
  SHA_CTX m_ctx;
};

inline void
Sha1::init() {
  SHA1_Init(&m_ctx);
}

inline void
Sha1::update(const void* data, unsigned int length) {
  SHA1_Update(&m_ctx, (const void*)data, length);
}

inline std::string
Sha1::final() {
  unsigned char buf[20];
  
  SHA1_Final(buf, &m_ctx);
  
  return std::string((char*)buf, 20);
}

#else
};
#endif

}

#endif

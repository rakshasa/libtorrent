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

#ifndef LIBTORRENT_HASH_COMPUTE_H
#define LIBTORRENT_HASH_COMPUTE_H

#include <cstring>

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

  void                final_c(char* buffer);

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

inline void
Sha1::final_c(char* buffer) {
  unsigned int len;
  
  SHA1_End(&m_ctx, (unsigned char*)buffer, &len, 20);
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

inline void
Sha1::final_c(char* buffer) {
  SHA1_Final((unsigned char*)buffer, &m_ctx);
}

#else
};
#endif

inline void
sha1_salt(const char* salt, unsigned int saltLength,
          const char* key, unsigned int keyLength,
          void* out) {
  Sha1 sha1;

  sha1.init();
  sha1.update(salt, saltLength);
  sha1.update(key, keyLength);
  sha1.final_c((char*)out);
}

inline void
sha1_salt(const char* salt, unsigned int saltLength,
          const char* key1, unsigned int key1Length,
          const char* key2, unsigned int key2Length,
          void* out) {
  Sha1 sha1;

  sha1.init();
  sha1.update(salt, saltLength);
  sha1.update(key1, key1Length);
  sha1.update(key2, key2Length);
  sha1.final_c((char*)out);
}

}

#endif

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

#ifndef LIBTORRENT_RC4_H
#define LIBTORRENT_RC4_H

#include "config.h"

#include <openssl/evp.h>

namespace torrent {

class RC4 {
public:
  RC4();
  ~RC4();
  RC4(const RC4&);
  RC4& operator=(const RC4&);

  RC4(const unsigned char key[], int len, int enc);

  void crypt(const void* indata, void* outdata, unsigned int length);
  void crypt(void* data, unsigned int length);

private:
  EVP_CIPHER_CTX* m_ctx{EVP_CIPHER_CTX_new()};
};

inline RC4::RC4() = default;

inline RC4::~RC4() {
  EVP_CIPHER_CTX_free(m_ctx);
}

inline RC4::RC4(const RC4& rhs) {
  EVP_CIPHER_CTX_copy(m_ctx, rhs.m_ctx);
}

inline RC4& RC4::operator=(const RC4& rhs) {
  if (this != &rhs) {
    EVP_CIPHER_CTX_free(m_ctx);
    auto ctx = EVP_CIPHER_CTX_new();
    EVP_CIPHER_CTX_copy(ctx, rhs.m_ctx);
    m_ctx = ctx;
  }
  return *this;
}

inline RC4::RC4(const unsigned char key[], int len, int enc) {
  EVP_CipherInit_ex(m_ctx, EVP_rc4(), nullptr, nullptr, nullptr, enc);
  EVP_CIPHER_CTX_set_key_length(m_ctx, len);
  EVP_CipherInit_ex(m_ctx, EVP_rc4(), nullptr, key, nullptr, -1);
}

inline void
RC4::crypt(const void* indata, void* outdata, unsigned int length) {
  int outlen;
  EVP_CipherUpdate(m_ctx, static_cast<unsigned char*>(outdata), &outlen, static_cast<const unsigned char*>(indata), length);
}

inline void
RC4::crypt(void* data, unsigned int length) {
  int outlen;
  EVP_CipherUpdate(m_ctx, static_cast<unsigned char*>(data), &outlen, static_cast<unsigned char*>(data), length);
}
} // namespace torrent

#endif

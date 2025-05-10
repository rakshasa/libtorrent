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

#ifndef LIBTORRENT_PROTOCOL_HANDSHAKE_ENCRYPTION_H
#define LIBTORRENT_PROTOCOL_HANDSHAKE_ENCRYPTION_H

#include <cstring>

#include "encryption_info.h"

namespace torrent {

class DiffieHellman;

class HandshakeEncryption {
public:
  enum Retry {
    RETRY_NONE,
    RETRY_PLAIN,
    RETRY_ENCRYPTED,
  };

  static constexpr int           crypto_plain = 1;
  static constexpr int           crypto_rc4   = 2;

  static const unsigned char dh_prime[];
  static constexpr unsigned int  dh_prime_length = 96;

  static const unsigned char dh_generator[];
  static constexpr unsigned int  dh_generator_length = 1;

  static const unsigned char vc_data[];
  static constexpr unsigned int  vc_length = 8;

  HandshakeEncryption(int options) :
    m_options(options)
    {}

  bool                has_crypto_plain() const                     { return m_crypto & crypto_plain; }
  bool                has_crypto_rc4() const                       { return m_crypto & crypto_rc4; }

  DiffieHellman*      key()                                        { return m_key; }
  EncryptionInfo*     info()                                       { return &m_info; }

  int                 options() const                              { return m_options; }

  int                 crypto() const                               { return m_crypto; }
  void                set_crypto(int val)                          { m_crypto = val; }

  Retry               retry() const                                { return m_retry; }
  void                set_retry(Retry val)                         { m_retry = val; }

  bool                should_retry() const;

  const char*         sync() const                                 { return m_sync; }
  unsigned int        sync_length() const                          { return m_syncLength; }

  void                set_sync(const char* src, unsigned int len)  { std::memcpy(m_sync, src, (m_syncLength = len)); }
  char*               modify_sync(unsigned int len)                { m_syncLength = len; return m_sync; }

  unsigned int        length_ia() const                            { return m_lengthIA; }
  void                set_length_ia(unsigned int len)              { m_lengthIA = len; }

  bool                initialize();
  void                cleanup();

  void                initialize_decrypt(const char* origHash, bool incoming);
  void                initialize_encrypt(const char* origHash, bool incoming);

  void                deobfuscate_hash(char* src) const;

  void                hash_req1_to_sync();
  void                encrypt_vc_to_sync(const char* origHash);

  static void         copy_vc(void* dest)                          { std::memset(dest, 0, vc_length); }
  static bool         compare_vc(const void* buf);

private:
  DiffieHellman*      m_key{};

  // A pointer instead?
  EncryptionInfo      m_info;

  int                 m_options;
  int                 m_crypto{0};

  Retry               m_retry{RETRY_NONE};

  char                m_sync[20];
  unsigned int        m_syncLength{0};

  unsigned int        m_lengthIA{0};
};

}

#endif

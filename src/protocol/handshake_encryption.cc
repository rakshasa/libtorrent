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

#include <sys/types.h>

#include <algorithm>
#include <functional>

#include "torrent/connection_manager.h"
#include "torrent/exceptions.h"
#include "utils/diffie_hellman.h"
#include "utils/sha1.h"

#include "handshake_encryption.h"

namespace torrent {

const unsigned char HandshakeEncryption::dh_prime[] = {
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xC9, 0x0F, 0xDA, 0xA2,
  0x21, 0x68, 0xC2, 0x34, 0xC4, 0xC6, 0x62, 0x8B, 0x80, 0xDC, 0x1C, 0xD1,
  0x29, 0x02, 0x4E, 0x08, 0x8A, 0x67, 0xCC, 0x74, 0x02, 0x0B, 0xBE, 0xA6,
  0x3B, 0x13, 0x9B, 0x22, 0x51, 0x4A, 0x08, 0x79, 0x8E, 0x34, 0x04, 0xDD,
  0xEF, 0x95, 0x19, 0xB3, 0xCD, 0x3A, 0x43, 0x1B, 0x30, 0x2B, 0x0A, 0x6D,
  0xF2, 0x5F, 0x14, 0x37, 0x4F, 0xE1, 0x35, 0x6D, 0x6D, 0x51, 0xC2, 0x45, 
  0xE4, 0x85, 0xB5, 0x76, 0x62, 0x5E, 0x7E, 0xC6, 0xF4, 0x4C, 0x42, 0xE9,
  0xA6, 0x3A, 0x36, 0x21, 0x00, 0x00, 0x00, 0x00, 0x00, 0x09, 0x05, 0x63, 
};

const unsigned char HandshakeEncryption::dh_generator[] = { 2 };
const unsigned char HandshakeEncryption::vc_data[] = { 0, 0, 0, 0, 0, 0, 0, 0 };

bool
HandshakeEncryption::should_retry() const {
  return (m_options & ConnectionManager::encryption_enable_retry) != 0 && m_retry != HandshakeEncryption::RETRY_NONE;
}

bool
HandshakeEncryption::initialize() {
  m_key = new DiffieHellman(dh_prime, dh_prime_length, dh_generator, dh_generator_length);

  return m_key->is_valid();
}

void
HandshakeEncryption::cleanup() {
  delete m_key;
  m_key = NULL;
}

bool
HandshakeEncryption::compare_vc(const void* buf) {
  return std::memcmp(buf, vc_data, vc_length) == 0;
}

void
HandshakeEncryption::initialize_decrypt(const char* origHash, bool incoming) {
  char hash[20];
  unsigned char discard[1024];

  sha1_salt(incoming ? "keyA" : "keyB", 4, m_key->c_str(), 96, origHash, 20, hash);

  m_info.set_decrypt(RC4(reinterpret_cast<const unsigned char*>(hash), 20));
  m_info.decrypt(discard, 1024);
}

void
HandshakeEncryption::initialize_encrypt(const char* origHash, bool incoming) {
  char hash[20];
  unsigned char discard[1024];

  sha1_salt(incoming ? "keyB" : "keyA", 4, m_key->c_str(), 96, origHash, 20, hash);

  m_info.set_encrypt(RC4(reinterpret_cast<const unsigned char*>(hash), 20));
  m_info.encrypt(discard, 1024);
}

// Obfuscated hash is HASH('req2', download_hash), extract that from
// HASH('req2', download_hash) ^ HASH('req3', S).
void
HandshakeEncryption::deobfuscate_hash(char* src) const {
  char tmp[20];
  sha1_salt("req3", 4, m_key->c_str(), m_key->size(), tmp);

  for (int i = 0; i < 20; i++)
    src[i] ^= tmp[i];
}

void
HandshakeEncryption::hash_req1_to_sync() {
  sha1_salt("req1", 4,
            m_key->c_str(), m_key->size(),
            modify_sync(20));
}

void
HandshakeEncryption::encrypt_vc_to_sync(const char* origHash) {
  m_syncLength = vc_length;
  std::memcpy(m_sync, vc_data, vc_length);

  char hash[20];
  char discard[1024];

  sha1_salt("keyB", 4, m_key->c_str(), 96, origHash, 20, hash);

  RC4 peerEncrypt(reinterpret_cast<const unsigned char*>(hash), 20);

  peerEncrypt.crypt(discard, 1024);
  peerEncrypt.crypt(m_sync, HandshakeEncryption::vc_length);
}

}

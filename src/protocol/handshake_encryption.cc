#include "config.h"

#include "handshake_encryption.h"

#include "torrent/net/network_config.h"
#include "utils/diffie_hellman.h"
#include "utils/sha1.h"

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
  return (m_options & net::NetworkConfig::encryption_enable_retry) != 0 && m_retry != HandshakeEncryption::RETRY_NONE;
}

HandshakeEncryption::HandshakeEncryption(int options) :
    m_options(options) {
}

bool
HandshakeEncryption::initialize() {
  m_key = std::make_unique<DiffieHellman>(dh_prime, dh_prime_length, dh_generator, dh_generator_length);

  return m_key->is_valid();
}

void
HandshakeEncryption::cleanup() {
  m_key = nullptr;
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

} // namespace torrent

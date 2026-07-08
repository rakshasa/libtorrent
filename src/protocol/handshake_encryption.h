#ifndef LIBTORRENT_PROTOCOL_HANDSHAKE_ENCRYPTION_H
#define LIBTORRENT_PROTOCOL_HANDSHAKE_ENCRYPTION_H

#include <cstdint>
#include <cstring>
#include <memory>

#include "encryption_info.h"
#include "torrent/runtime/encryption_policy.h"

namespace torrent {

class DiffieHellman;

class HandshakeEncryption {
public:
  static constexpr int           crypto_plain = 1;
  static constexpr int           crypto_rc4   = 2;

  static const unsigned char dh_prime[];
  static constexpr unsigned int  dh_prime_length = 96;

  static const unsigned char dh_generator[];
  static constexpr unsigned int  dh_generator_length = 1;

  static const unsigned char vc_data[];
  static constexpr unsigned int  vc_length = 8;

  HandshakeEncryption(EncryptionPolicy policy);

  const auto&         key()                                        { return m_key; }
  EncryptionInfo*     info()                                       { return &m_info; }

  const EncryptionPolicy& policy() const                           { return m_policy; }

  void                disarm_outgoing_alt_retry()                  { m_policy.disarm_outgoing_alt_retry(); }

  int                 crypto() const                               { return m_crypto; }
  void                set_crypto(int val)                          { m_crypto = val; }

  static const char*  crypto_name(int crypto);

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
  std::unique_ptr<DiffieHellman> m_key;

  // A pointer instead?
  EncryptionInfo      m_info;

  EncryptionPolicy    m_policy;
  int                 m_crypto{0};

  char                m_sync[20];
  unsigned int        m_syncLength{0};

  unsigned int        m_lengthIA{0};
};

} // namespace torrent

#endif

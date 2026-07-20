#ifndef LIBTORRENT_PROTOCOL_HANDSHAKE_ENCRYPTION_H
#define LIBTORRENT_PROTOCOL_HANDSHAKE_ENCRYPTION_H

#include <cstdint>
#include <cstring>
#include <memory>

#include "protocol/encryption_info.h"
#include "protocol/encryption_policy.h"

namespace torrent {

class DiffieHellman;

class HandshakeEncryption {
public:
  static constexpr int           crypto_plaintext = 0x1;
  static constexpr int           crypto_encrypted = 0x2;

  static const unsigned char     dh_prime[];
  static constexpr unsigned int  dh_prime_length = 96;

  static const unsigned char     dh_generator[];
  static constexpr unsigned int  dh_generator_length = 1;

  static const unsigned char     vc_data[];
  static constexpr unsigned int  vc_length = 8;

  bool                is_stream_encrypted() const;
  bool                is_stream_plaintext() const;

  bool                has_stream_both() const;
  bool                has_stream_encrypted() const;
  bool                has_stream_plaintext() const;

  const auto&         key()                                        { return m_key; }
  EncryptionInfo*     info()                                       { return &m_info; }

  auto&               policy();
  void                set_policy(const EncryptionPolicy& policy);

  int                 crypto() const                               { return m_crypto; }
  void                set_crypto(int val)                          { m_crypto = val; }

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

inline bool  HandshakeEncryption::is_stream_encrypted() const                { return m_crypto == crypto_encrypted; }
inline bool  HandshakeEncryption::is_stream_plaintext() const                { return m_crypto == crypto_plaintext; }
inline bool  HandshakeEncryption::has_stream_both() const                    { return has_stream_plaintext() && has_stream_encrypted(); }
inline bool  HandshakeEncryption::has_stream_encrypted() const               { return (m_crypto & crypto_encrypted) != 0; }
inline bool  HandshakeEncryption::has_stream_plaintext() const               { return (m_crypto & crypto_plaintext) != 0; }

inline auto& HandshakeEncryption::policy()                                   { return m_policy; }
inline void  HandshakeEncryption::set_policy(const EncryptionPolicy& policy) { m_policy = policy; }

} // namespace torrent

#endif

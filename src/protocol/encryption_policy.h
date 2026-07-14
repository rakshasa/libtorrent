#ifndef LIBTORRENT_PROTOCOL_ENCRYPTION_POLICY_H
#define LIBTORRENT_PROTOCOL_ENCRYPTION_POLICY_H

#include <cstdint>
#include <string>
#include <torrent/common.h>

namespace torrent {

class LIBTORRENT_EXPORT EncryptionPolicy {
public:
  EncryptionPolicy() = default;
  EncryptionPolicy(encryption_mode handshake_mode, encryption_mode stream_mode);
  EncryptionPolicy(std::pair<encryption_mode, encryption_mode> encryption_modes);

  bool                is_retrying() const;

  void                set_retrying();
  void                set_retry_disabled();
  void                set_retry_plaintext();
  void                set_retry_encrypted();

  encryption_mode     handshake_mode() const;
  encryption_mode     stream_mode() const;

  const char*         handshake_c_str() const;
  const char*         stream_c_str() const;

  bool                allow_plaintext_handshake() const;
  bool                allow_encrypted_handshake() const;
  bool                prefer_encrypted_handshake() const;
  bool                require_encrypted_handshake() const;

  bool                allow_plaintext_stream() const;
  bool                allow_encrypted_stream() const;
  bool                prefer_encrypted_stream() const;
  bool                require_plaintext_stream() const;
  bool                require_encrypted_stream() const;

  bool                retry_plaintext_handshake() const;
  bool                retry_encrypted_handshake() const;

private:
  bool                m_retrying{};

  encryption_mode     m_handshake_mode{ENCRYPTION_MODE_ALLOW};
  encryption_mode     m_stream_mode{ENCRYPTION_MODE_ALLOW};
  encryption_mode     m_retry_mode{ENCRYPTION_MODE_ALLOW};
};

inline bool EncryptionPolicy::is_retrying() const                 { return m_retrying; }
inline void EncryptionPolicy::set_retrying()                      { m_retrying = true; }

inline void EncryptionPolicy::set_retry_disabled()                { m_retry_mode = ENCRYPTION_MODE_PREFER; }
inline void EncryptionPolicy::set_retry_plaintext()               { m_retry_mode = ENCRYPTION_MODE_DENY; }
inline void EncryptionPolicy::set_retry_encrypted()               { m_retry_mode = ENCRYPTION_MODE_REQUIRE; }

inline encryption_mode EncryptionPolicy::handshake_mode() const   { return m_handshake_mode; }
inline encryption_mode EncryptionPolicy::stream_mode() const      { return m_stream_mode; }

inline bool EncryptionPolicy::allow_plaintext_handshake() const   { return m_handshake_mode != ENCRYPTION_MODE_REQUIRE; }
inline bool EncryptionPolicy::allow_encrypted_handshake() const   { return m_handshake_mode != ENCRYPTION_MODE_DENY; }
inline bool EncryptionPolicy::prefer_encrypted_handshake() const  { return m_handshake_mode == ENCRYPTION_MODE_PREFER || m_handshake_mode == ENCRYPTION_MODE_REQUIRE; }
inline bool EncryptionPolicy::require_encrypted_handshake() const { return m_handshake_mode == ENCRYPTION_MODE_REQUIRE; }

inline bool EncryptionPolicy::allow_plaintext_stream() const      { return m_stream_mode != ENCRYPTION_MODE_REQUIRE; }
inline bool EncryptionPolicy::allow_encrypted_stream() const      { return m_stream_mode != ENCRYPTION_MODE_DENY; }
inline bool EncryptionPolicy::prefer_encrypted_stream() const     { return m_stream_mode == ENCRYPTION_MODE_PREFER || m_stream_mode == ENCRYPTION_MODE_REQUIRE; }
inline bool EncryptionPolicy::require_plaintext_stream() const    { return m_stream_mode == ENCRYPTION_MODE_DENY; }
inline bool EncryptionPolicy::require_encrypted_stream() const    { return m_stream_mode == ENCRYPTION_MODE_REQUIRE; }

inline bool EncryptionPolicy::retry_plaintext_handshake() const   { return m_retry_mode == ENCRYPTION_MODE_DENY; }
inline bool EncryptionPolicy::retry_encrypted_handshake() const   { return m_retry_mode == ENCRYPTION_MODE_REQUIRE; }

} // namespace torrent

#endif

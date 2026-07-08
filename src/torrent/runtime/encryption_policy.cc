#include "config.h"

#include "torrent/runtime/encryption_policy.h"

#include "protocol/handshake_encryption.h"
#include "torrent/utils/option_strings.h"

namespace torrent {

const char*
EncryptionPolicy::mode_name(Mode value) {
  return option_to_c_str(OPTION_ENCRYPTION_MODE, static_cast<unsigned int>(value));
}

EncryptionPolicy::Mode
EncryptionPolicy::parse_mode(const std::string& value) {
  return static_cast<Mode>(option_find_string_str(OPTION_ENCRYPTION_MODE, value));
}

bool
EncryptionPolicy::outgoing_uses_pe() const {
  return handshake == Mode::require
      || (handshake == Mode::prefer && !is_outgoing_retry)
      || (handshake == Mode::allow && is_outgoing_retry);
}

bool
EncryptionPolicy::should_outgoing_alt_retry() const {
  return outgoing_alt_retry_armed
      && !is_outgoing_retry
      && (handshake == Mode::allow || handshake == Mode::prefer);
}

int
EncryptionPolicy::outgoing_crypto_offer() const {
  switch (stream) {
  case Mode::require:
    return HandshakeEncryption::crypto_rc4;
  case Mode::deny:
    return HandshakeEncryption::crypto_plain;
  case Mode::allow:
  case Mode::prefer:
    return HandshakeEncryption::crypto_plain | HandshakeEncryption::crypto_rc4;
  }

  return HandshakeEncryption::crypto_plain;
}

int
EncryptionPolicy::incoming_crypto_select(int offer) const {
  const int plain = HandshakeEncryption::crypto_plain;
  const int rc4   = HandshakeEncryption::crypto_rc4;

  switch (stream) {
  case Mode::deny:
    return (offer & plain) ? plain : 0;

  case Mode::require:
    return (offer & rc4) ? rc4 : 0;

  case Mode::allow:
    if (offer & plain)
      return plain;
    if (offer & rc4)
      return rc4;
    return 0;

  case Mode::prefer:
    if (offer & rc4)
      return rc4;
    if (offer & plain)
      return plain;
    return 0;
  }

  return 0;
}

std::string
EncryptionPolicy::to_string() const {
  return std::string("handshake=") + mode_name(handshake)
       + ",stream=" + mode_name(stream);
}

} // namespace torrent
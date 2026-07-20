#include "config.h"

#include "protocol/encryption_policy.h"

#include "torrent/exceptions.h"
#include "torrent/utils/option_strings.h"

namespace torrent {

namespace {

void
validate_modes(EncryptionPolicy* policy) {
  if (policy->handshake_mode() == ENCRYPTION_MODE_DENY && policy->stream_mode() == ENCRYPTION_MODE_REQUIRE)
    throw internal_error("EncryptionPolicy::validate_modes() Invalid mode: handshake_deny/stream_require");
}

} // namespace anonymous


EncryptionPolicy::EncryptionPolicy(encryption_mode handshake_mode, encryption_mode stream_mode)
  : m_handshake_mode(handshake_mode),
    m_stream_mode(stream_mode) {

  validate_modes(this);
}

EncryptionPolicy::EncryptionPolicy(std::pair<encryption_mode, encryption_mode> encryption_modes)
  : m_handshake_mode(encryption_modes.first),
    m_stream_mode(encryption_modes.second) {

  validate_modes(this);
}

const char*
EncryptionPolicy::handshake_c_str() const {
  return option_to_c_str_or_throw(OPTION_ENCRYPTION_HANDSHAKE, m_handshake_mode);
}

const char*
EncryptionPolicy::stream_c_str() const {
  return option_to_c_str_or_throw(OPTION_ENCRYPTION_STREAM, m_stream_mode);
}

} // namespace torrent

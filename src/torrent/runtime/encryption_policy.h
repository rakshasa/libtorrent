#ifndef LIBTORRENT_TORRENT_RUNTIME_ENCRYPTION_POLICY_H
#define LIBTORRENT_TORRENT_RUNTIME_ENCRYPTION_POLICY_H

#include <cstdint>
#include <string>

#include <torrent/common.h>

namespace torrent {

enum encryption_mode_enum : uint8_t {
  ENCRYPTION_MODE_DENY,
  ENCRYPTION_MODE_ALLOW,
  ENCRYPTION_MODE_PREFER,
  ENCRYPTION_MODE_REQUIRE,
};

class LIBTORRENT_EXPORT EncryptionPolicy {
public:
  enum class Mode : uint8_t {
    deny    = ENCRYPTION_MODE_DENY,
    allow   = ENCRYPTION_MODE_ALLOW,
    prefer  = ENCRYPTION_MODE_PREFER,
    require = ENCRYPTION_MODE_REQUIRE,
  };

  Mode handshake{Mode::allow};
  Mode stream{Mode::allow};

  bool      is_outgoing_retry{false};
  bool      outgoing_alt_retry_armed{true};

  bool      outgoing_uses_pe() const;
  bool      should_outgoing_alt_retry() const;
  void      disarm_outgoing_alt_retry()                  { outgoing_alt_retry_armed = false; }
  int       outgoing_crypto_offer() const;
  int       incoming_crypto_select(int offer) const;

  std::string to_string() const;

  static const char* mode_name(Mode value);
  static Mode        parse_mode(const std::string& value);

  static EncryptionPolicy disabled() {
    return {
      .handshake = Mode::deny,
      .stream    = Mode::deny,
    };
  }
};

} // namespace torrent

#endif
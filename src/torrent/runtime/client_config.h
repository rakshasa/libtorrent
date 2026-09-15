#ifndef LIBTORRENT_TORRENT_RUNTIME_CLIENT_CONFIG_H
#define LIBTORRENT_TORRENT_RUNTIME_CLIENT_CONFIG_H

#include <mutex>
#include <torrent/common.h>

namespace torrent::runtime {

ClientConfig* client_config() LIBTORRENT_EXPORT;

class LIBTORRENT_EXPORT ClientConfig {
public:
  using port_range = std::pair<uint16_t, uint16_t>;

  port_range          listen_port_range() const;
  void                set_listen_port_range(uint16_t first, uint16_t last);

  bool                listen_port_random() const;
  void                set_listen_port_random(bool v);

  bool                is_pex_enabled() const;
  void                set_pex_enabled(bool v);

  bool                torrent_name_use_sanitized() const;
  void                set_torrent_name_use_sanitized(bool v);

  std::string         file_name_replace_slash() const;
  void                set_file_name_replace_slash(const std::string& str);

protected:
  friend class torrent::RuntimeManager;

  auto                lock_guard() const;

private:
  ClientConfig() = default;
  ~ClientConfig() = default;

  mutable std::mutex  m_mutex;

  uint16_t            m_listen_port_first{6881};
  uint16_t            m_listen_port_last{6999};

  std::string         m_file_name_replace_slash{"_"};

  align_cacheline

  std::atomic<bool>   m_listen_port_random{true};
  std::atomic<bool>   m_pex_enabled{true};
  std::atomic<bool>   m_torrent_name_use_sanitized{true};
};

inline auto ClientConfig::lock_guard() const                     { return std::lock_guard(m_mutex); }

inline bool ClientConfig::listen_port_random() const             { return m_listen_port_random; }
inline void ClientConfig::set_listen_port_random(bool v)         { m_listen_port_random = v; }

inline bool ClientConfig::is_pex_enabled() const                 { return m_pex_enabled; }
inline void ClientConfig::set_pex_enabled(bool v)                { m_pex_enabled = v; }

inline bool ClientConfig::torrent_name_use_sanitized() const     { return m_torrent_name_use_sanitized; }
inline void ClientConfig::set_torrent_name_use_sanitized(bool v) { m_torrent_name_use_sanitized = v; }

inline std::string ClientConfig::file_name_replace_slash() const { auto guard = lock_guard(); return m_file_name_replace_slash; }

} // namespace torrent::runtime

#endif

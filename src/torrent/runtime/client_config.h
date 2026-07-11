#ifndef LIBTORRENT_TORRENT_RUNTIME_CLIENT_CONFIG_H
#define LIBTORRENT_TORRENT_RUNTIME_CLIENT_CONFIG_H

#include <mutex>
#include <torrent/common.h>

namespace torrent::runtime {

ClientConfig* client_config() LIBTORRENT_EXPORT;

class LIBTORRENT_EXPORT ClientConfig {
public:

  auto                listen_port_range() const;
  void                set_listen_port_range(uint16_t first, uint16_t last);

  bool                listen_port_random() const;
  void                set_listen_port_random(bool v);

protected:
  friend class torrent::RuntimeManager;

  auto                lock_guard() const;

private:
  ClientConfig();
  ~ClientConfig();

  mutable std::mutex  m_mutex;

  uint16_t            m_listen_port_first{6881};
  uint16_t            m_listen_port_last{6999};
  bool                m_listen_port_random{true};
};

inline auto ClientConfig::lock_guard() const             { return std::lock_guard(m_mutex); }
inline auto ClientConfig::listen_port_range() const      { auto guard = lock_guard(); return std::make_pair(m_listen_port_first, m_listen_port_last); }
inline bool ClientConfig::listen_port_random() const     { auto guard = lock_guard(); return m_listen_port_random; }
inline void ClientConfig::set_listen_port_random(bool v) { auto guard = lock_guard(); m_listen_port_random = v; }

} // namespace torrent::runtime

#endif

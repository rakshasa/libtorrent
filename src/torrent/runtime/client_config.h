#ifndef LIBTORRENT_TORRENT_RUNTIME_CLIENT_CONFIG_H
#define LIBTORRENT_TORRENT_RUNTIME_CLIENT_CONFIG_H

#include <mutex>
#include <torrent/common.h>

namespace torrent::runtime {

NetworkConfig* network_config() LIBTORRENT_EXPORT;

class LIBTORRENT_EXPORT ClientConfig {
public:

  bool                port_random() const;
  void                set_port_random(bool v);

  auto                port_range() const;
  void                set_port_range(uint16_t first, uint16_t last);

protected:
  friend class torrent::RuntimeManager;

  auto                lock_guard() const              { return std::lock_guard(m_mutex); }

private:
  ClientConfig();
  ~ClientConfig();

  mutable std::mutex  m_mutex;

  bool                m_port_random;
  uint16_t            m_port_first;
  uint16_t            m_port_last;
};

inline bool ClientConfig::port_random() const                           { auto guard = lock_guard(); return m_port_random; }
inline void ClientConfig::set_port_random(bool v)                       { auto guard = lock_guard(); m_port_random = v; }
inline auto ClientConfig::port_range() const                            { auto guard = lock_guard(); return std::make_pair(m_port_first, m_port_last); }
inline void ClientConfig::set_port_range(uint16_t first, uint16_t last) { auto guard = lock_guard(); m_port_first = first; m_port_last = last; }

} // namespace torrent::runtime

#endif

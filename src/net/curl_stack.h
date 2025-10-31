#ifndef RTORRENT_CORE_CURL_STACK_H
#define RTORRENT_CORE_CURL_STACK_H

#include <memory>
#include <mutex>
#include <new>
#include <string>
#include <vector>
#include <curl/curl.h>

#include "torrent/utils/scheduler.h"

namespace torrent::net {

class CurlGet;
class CurlSocket;

// Since std::hardware_destructive_interference_size only got added in gcc 12.1

class alignas(LT_SMP_CACHE_BYTES) CurlStack : private std::vector<std::shared_ptr<CurlGet>> {
public:
  using base_type = std::vector<std::shared_ptr<CurlGet>>;

  CurlStack(utils::Thread* thread);
  ~CurlStack();

  bool                is_running() const;

  unsigned int        size() const;

  unsigned int        max_cache_connections() const;
  unsigned int        max_host_connections() const;
  unsigned int        max_total_connections() const;

  void                set_max_cache_connections(unsigned int value);
  void                set_max_host_connections(unsigned int value);
  void                set_max_total_connections(unsigned int value);

  const std::string&  user_agent() const;
  const std::string&  http_proxy() const;
  const std::string&  http_capath() const;
  const std::string&  http_cacert() const;

  void                set_user_agent(const std::string& s);
  void                set_http_proxy(const std::string& s);
  void                set_http_capath(const std::string& s);
  void                set_http_cacert(const std::string& s);

  bool                ssl_verify_host() const;
  bool                ssl_verify_peer() const;
  void                set_ssl_verify_host(bool s);
  void                set_ssl_verify_peer(bool s);

  long                dns_timeout() const;
  void                set_dns_timeout(long timeout);

  void                shutdown();

  void                start_get(const std::shared_ptr<CurlGet>& curl_get);
  void                close_get(const std::shared_ptr<CurlGet>& curl_get);

  utils::Thread*      thread() const                         { return m_thread; }

protected:
  friend class CurlGet;
  friend class CurlSocket;

  CURLM*              handle() const                         { return m_handle; }

  // We need to lock when changing any of the values publically accessible.
  void                lock() const                           { m_mutex.lock(); }
  auto                lock_guard() const                     { return std::scoped_lock(m_mutex); }
  void                unlock() const                         { m_mutex.unlock(); }

private:
  CurlStack(const CurlStack&) = delete;
  CurlStack operator=(const CurlStack&) = delete;

  base_type::iterator find_curl_handle(const CURL* curl_handle);

  static int          set_timeout(void*, long timeout_ms, CurlStack* stack);

  void                receive_timeout();
  bool                process_done_handle();

  // Unprotected members (including base_type vector), only changed in ways that are implicitly
  // thread-safe. E.g. before any threads are started or only within the owning thread.
  utils::Thread*        m_thread{};
  CURLM*                m_handle{};
  utils::SchedulerEntry m_task_timeout;

  mutable std::mutex  m_mutex;

  // Use lock guard when accessing these members, and when modifying the underlying vector.
  bool                m_running{true};

  unsigned int        m_max_cache_connections{0};
  unsigned int        m_max_host_connections{0};
  unsigned int        m_max_total_connections{32};

  std::string         m_user_agent;
  std::string         m_http_proxy;
  std::string         m_http_ca_path;
  std::string         m_http_ca_cert;

  bool                m_ssl_verify_host{true};
  bool                m_ssl_verify_peer{true};
  long                m_dns_timeout{60};
};

inline bool
CurlStack::is_running() const {
  auto guard = lock_guard();
  return m_running;
}

inline unsigned int
CurlStack::size() const {
  auto guard = lock_guard();
  return base_type::size();
}

inline unsigned int
CurlStack::max_cache_connections() const {
  auto guard = lock_guard();
  return m_max_cache_connections;
}

inline unsigned int
CurlStack::max_host_connections() const {
  auto guard = lock_guard();
  return m_max_host_connections;
}

inline unsigned int
CurlStack::max_total_connections() const {
  auto guard = lock_guard();
  return m_max_total_connections;
}

inline const std::string&
CurlStack::user_agent() const {
  auto guard = lock_guard();
  return m_user_agent;
}

inline const std::string&
CurlStack::http_proxy() const {
  auto guard = lock_guard();
  return m_http_proxy;
}

inline const std::string&
CurlStack::http_capath() const {
  auto guard = lock_guard();
  return m_http_ca_path;
}

inline const std::string&
CurlStack::http_cacert() const {
  auto guard = lock_guard();
  return m_http_ca_cert;
}

inline void
CurlStack::set_user_agent(const std::string& s) {
  auto guard = lock_guard();
  m_user_agent = s;
}

inline void
CurlStack::set_http_proxy(const std::string& s) {
  auto guard = lock_guard();
  m_http_proxy = s;
}

inline void
CurlStack::set_http_capath(const std::string& s) {
  auto guard = lock_guard();
  m_http_ca_path = s;
}

inline void
CurlStack::set_http_cacert(const std::string& s) {
  auto guard = lock_guard();
  m_http_ca_cert = s;
}

inline bool
CurlStack::ssl_verify_host() const {
  auto guard = lock_guard();
  return m_ssl_verify_host;
}

inline bool
CurlStack::ssl_verify_peer() const {
  auto guard = lock_guard();
  return m_ssl_verify_peer;
}

inline void
CurlStack::set_ssl_verify_host(bool s) {
  auto guard = lock_guard();
  m_ssl_verify_host = s;
}

inline void
CurlStack::set_ssl_verify_peer(bool s) {
  auto guard = lock_guard();
  m_ssl_verify_peer = s;
}

inline long
CurlStack::dns_timeout() const {
  auto guard = lock_guard();
  return m_dns_timeout;
}

inline void
CurlStack::set_dns_timeout(long timeout) {
  auto guard = lock_guard();
  m_dns_timeout = timeout;
}

} // namespace torrent::net

#endif

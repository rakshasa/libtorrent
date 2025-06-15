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

// Since std::hardware_destructive_interference_size only got added in gcc 12.1, we use 256 which
// should be more than enough.

class alignas(256) CurlStack : private std::vector<std::shared_ptr<CurlGet>> {
public:
  using base_type = std::vector<std::shared_ptr<CurlGet>>;

  CurlStack();
  ~CurlStack();

  bool                is_running() const;

  unsigned int        active() const;
  unsigned int        max_active() const;
  void                set_max_active(unsigned int a);

  const std::string&  user_agent() const;
  const std::string&  http_proxy() const;
  const std::string&  bind_address() const;
  const std::string&  http_capath() const;
  const std::string&  http_cacert() const;

  void                set_user_agent(const std::string& s);
  void                set_http_proxy(const std::string& s);
  void                set_bind_address(const std::string& s);
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
  CURLM*                m_handle{};
  utils::SchedulerEntry m_task_timeout;

  mutable std::mutex  m_mutex;

  // Use lock guard when accessing these members, and when modifying the underlying vector.
  bool                m_running{true};
  unsigned int        m_active{0};
  unsigned int        m_max_active{32};

  std::string         m_user_agent;
  std::string         m_http_proxy;
  std::string         m_bind_address;
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
CurlStack::active() const {
  auto guard = lock_guard();
  return m_active;
}

inline unsigned int
CurlStack::max_active() const {
  auto guard = lock_guard();
  return m_max_active;
}

inline void
CurlStack::set_max_active(unsigned int a) {
  auto guard = lock_guard();
  m_max_active = a;
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
CurlStack::bind_address() const {
  auto guard = lock_guard();
  return m_bind_address;
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
CurlStack::set_bind_address(const std::string& s) {
  auto guard = lock_guard();
  m_bind_address = s;
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

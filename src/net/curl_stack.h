#ifndef RTORRENT_CORE_CURL_STACK_H
#define RTORRENT_CORE_CURL_STACK_H

#include <string>
#include <vector>

#include "torrent/utils/scheduler.h"

namespace torrent::net {

class CurlGet;
class CurlSocket;

// By using a deque instead of vector we allow for cheaper removal of
// the oldest elements, those that will be first in the in the
// deque.
//
// This should fit well with the use-case of a http stack, thus
// we get most of the cache locality benefits of a vector with fast
// removal of elements.

class CurlStack : private std::vector<CurlGet*> {
public:
  using base_type = std::vector<CurlGet*>;

  CurlStack();
  ~CurlStack();

  bool                is_running() const;

  void                shutdown();

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

  void                receive_action(CurlSocket* socket, int type);

protected:
  friend class CurlGet;
  friend class CurlSocket;

  void                lock() const                           { m_mutex.lock(); }
  auto                lock_guard() const                     { return std::scoped_lock(m_mutex); }
  void                unlock() const                         { m_mutex.unlock(); }

  void                add_get(CurlGet* get);
  void                remove_get(CurlGet* get);

  void                transfer_done(void* handle, const char* msg);

  void*               handle() const                         { return m_handle; }

private:
  CurlStack(const CurlStack&) = delete;
  void operator = (const CurlStack&) = delete;

  static int          set_timeout(void*, long timeout_ms, void* userp);

  void                receive_timeout();
  bool                process_done_handle();

  base_type::iterator find_curl_get(CurlGet* curl_get);

  void*                          m_handle;
  torrent::utils::SchedulerEntry m_task_timeout;

  // Use lock guard when accessing these members, and when modifying the underlying vector.
  mutable std::mutex  m_mutex;

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

bool
CurlStack::is_running() const {
  auto guard = lock_guard();
  return m_running;
}

unsigned int
CurlStack::active() const {
  auto guard = lock_guard();
  return m_active;
}

unsigned int
CurlStack::max_active() const {
  auto guard = lock_guard();
  return m_max_active;
}

void
CurlStack::set_max_active(unsigned int a) {
  auto guard = lock_guard();
  m_max_active = a;
}

const std::string&
CurlStack::user_agent() const {
  auto guard = lock_guard();
  return m_user_agent;
}

const std::string&
CurlStack::http_proxy() const {
  auto guard = lock_guard();
  return m_http_proxy;
}

const std::string&
CurlStack::bind_address() const {
  auto guard = lock_guard();
  return m_bind_address;
}

const std::string&
CurlStack::http_capath() const {
  auto guard = lock_guard();
  return m_http_ca_path;
}

const std::string&
CurlStack::http_cacert() const {
  auto guard = lock_guard();
  return m_http_ca_cert;
}

void
CurlStack::set_user_agent(const std::string& s) {
  auto guard = lock_guard();
  m_user_agent = s;
}

void
CurlStack::set_http_proxy(const std::string& s) {
  auto guard = lock_guard();
  m_http_proxy = s;
}

void
CurlStack::set_bind_address(const std::string& s) {
  auto guard = lock_guard();
  m_bind_address = s;
}

void
CurlStack::set_http_capath(const std::string& s) {
  auto guard = lock_guard();
  m_http_ca_path = s;
}

void
CurlStack::set_http_cacert(const std::string& s) {
  auto guard = lock_guard();
  m_http_ca_cert = s;
}

bool
CurlStack::ssl_verify_host() const {
  auto guard = lock_guard();
  return m_ssl_verify_host;
}

bool
CurlStack::ssl_verify_peer() const {
  auto guard = lock_guard();
  return m_ssl_verify_peer;
}

void
CurlStack::set_ssl_verify_host(bool s) {
  auto guard = lock_guard();
  m_ssl_verify_host = s;
}

void
CurlStack::set_ssl_verify_peer(bool s) {
  auto guard = lock_guard();
  m_ssl_verify_peer = s;
}

long
CurlStack::dns_timeout() const {
  auto guard = lock_guard();
  return m_dns_timeout;
}

void
CurlStack::set_dns_timeout(long timeout) {
  auto guard = lock_guard();
  m_dns_timeout = timeout;
}

}

#endif

#ifndef RTORRENT_CORE_CURL_GET_H
#define RTORRENT_CORE_CURL_GET_H

#include <iosfwd>
#include <list>
#include <string>
#include <curl/curl.h>

#include "torrent/utils/scheduler.h"

namespace torrent::net {

class CurlStack;

class CurlGet {
public:
  CurlGet();
  ~CurlGet();

  bool               is_active() const;
  bool               is_busy() const;
  bool               is_using_ipv6() const;

  // TODO: Don't allow getting the stream.
  const std::string& url() const;
  std::iostream*     stream();
  uint32_t           timeout() const;

  // Make sure the output stream does not have any bad/failed bits set.
  void               set_url(const std::string& url);
  void               set_stream(std::iostream* str);
  // TODO: Change to std::chrono.
  void               set_timeout(uint32_t seconds);

  void               retry_ipv6();

  int64_t            size_done();
  int64_t            size_total();

  // TODO: This isn't thread-safe, and HttpGet needs to be fixed.
  CURL*              handle()                        { return m_handle; }
  CurlStack*         curl_stack()                    { return m_stack; }

  // The owner of the Http object must close it as soon as possible
  // after receiving the signal, as the implementation may allocate
  // limited resources during its lifetime.
  auto&              signal_done()                   { return m_signal_done; }
  auto&              signal_failed()                 { return m_signal_failed; }

protected:
  friend class CurlStack;

  // We need to lock when changing any of the values publically accessible. This means we don't need
  // to lock when changing the underlying vector.
  void               lock() const                    { m_mutex.lock(); }
  auto               lock_guard() const              { return std::scoped_lock(m_mutex); }
  void               unlock() const                  { m_mutex.unlock(); }

  void               prepare_start(CurlStack* stack);
  void               activate();
  void               cleanup();

  void               receive_timeout();

  void               trigger_done();
  void               trigger_failed(const std::string& message);

private:
  CurlGet(const CurlGet&) = delete;
  void operator = (const CurlGet&) = delete;

  CURL*              m_handle{};
  CurlStack*         m_stack;

  bool               m_ipv6;

  torrent::utils::SchedulerEntry m_task_timeout;

  mutable std::mutex  m_mutex;

  // When you change timeout to a different type, update curl_get.cc where it multiplies 1s*m_timeout.

  bool               m_active{};
  uint32_t           m_timeout{};

  std::string        m_url;
  // TODO: Use shared_ptr, or replace with std::function.
  std::iostream*     m_stream{};

  std::list<std::function<void()>>                   m_signal_done;
  std::list<std::function<void(const std::string&)>> m_signal_failed;
};

bool
CurlGet::is_active() const {
  auto guard = lock_guard();
  return m_active;
}

bool
CurlGet::is_busy() const {
  auto guard = lock_guard();
  return m_handle != nullptr;
}

bool
CurlGet::is_using_ipv6() const {
  auto guard = lock_guard();
  return m_ipv6;
}

const std::string&
CurlGet::url() const {
  auto guard = lock_guard();
  return m_url;
}

std::iostream*
CurlGet::stream() {
  auto guard = lock_guard();
  return m_stream;
}

uint32_t
CurlGet::timeout() const {
  auto guard = lock_guard();
  return m_timeout;
}

}

#endif

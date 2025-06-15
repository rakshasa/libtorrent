#ifndef RTORRENT_CORE_CURL_GET_H
#define RTORRENT_CORE_CURL_GET_H

#include <iosfwd>
#include <list>
#include <mutex>
#include <string>
#include <curl/curl.h>

#include "torrent/utils/scheduler.h"

namespace torrent::net {

class CurlStack;

class CurlGet {
public:
  CurlGet();
  ~CurlGet();

  bool                is_stacked() const;
  bool                is_active() const;
  bool                is_using_ipv6() const;

  CurlStack*          curl_stack() const;

  // TODO: Don't allow getting the stream.
  const std::string&  url() const;
  std::iostream*      stream();
  uint32_t            timeout() const;

  // Make sure the output stream does not have any bad/failed bits set.
  void                set_url(const std::string& url);
  void                set_stream(std::iostream* str);
  // TODO: Change to std::chrono.
  void                set_timeout(uint32_t seconds);

  int64_t             size_done();
  int64_t             size_total();

  // The owner of the Http object must close it as soon as possible
  // after receiving the signal, as the implementation may allocate
  // limited resources during its lifetime.
  void                add_done_slot(const std::function<void()>& slot);
  void                add_failed_slot(const std::function<void(const std::string&)>& slot);

protected:
  friend class CurlStack;

  // We need to lock when changing any of the values publically accessible. This means we don't need
  // to lock when changing the underlying vector.
  void                lock() const                    { m_mutex.lock(); }
  auto                lock_guard() const              { return std::scoped_lock(m_mutex); }
  void                unlock() const                  { m_mutex.unlock(); }
  auto&               mutex() const                   { return m_mutex; }

  bool                is_active_unsafe() const        { return m_active; }
  auto                handle_unsafe() const           { return m_handle; }

  void                prepare_start_unsafe(CurlStack* stack);
  void                activate_unsafe();
  void                cleanup_unsafe();

  // TODO: No locking required?
  void                retry_ipv6();

  void                trigger_done();
  void                trigger_failed(const std::string& message);

private:
  CurlGet(const CurlGet&) = delete;
  CurlGet& operator=(const CurlGet&) = delete;

  mutable std::mutex  m_mutex;

  CURL*               m_handle{};
  CurlStack*          m_stack;

  bool                m_ipv6;

  torrent::utils::SchedulerEntry m_task_timeout;

  // When you change timeout to a different type, update curl_get.cc where it multiplies 1s*m_timeout.

  bool                m_active{};
  uint32_t            m_timeout{};

  std::string         m_url;
  // TODO: Use shared_ptr, or replace with std::function.
  std::iostream*      m_stream{};

  std::list<std::function<void()>>                   m_signal_done;
  std::list<std::function<void(const std::string&)>> m_signal_failed;
};

inline bool
CurlGet::is_stacked() const {
  auto guard = lock_guard();
  return m_handle != nullptr;
}

inline bool
CurlGet::is_active() const {
  auto guard = lock_guard();
  return m_active;
}

inline bool
CurlGet::is_using_ipv6() const {
  auto guard = lock_guard();
  return m_ipv6;
}

inline CurlStack*
CurlGet::curl_stack() const {
  auto guard = lock_guard();
  return m_stack;
}

inline const std::string&
CurlGet::url() const {
  auto guard = lock_guard();
  return m_url;
}

inline std::iostream*
CurlGet::stream() {
  auto guard = lock_guard();
  return m_stream;
}

inline uint32_t
CurlGet::timeout() const {
  auto guard = lock_guard();
  return m_timeout;
}

} // namespace torrent::net

#endif

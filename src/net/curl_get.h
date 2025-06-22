#ifndef RTORRENT_CORE_CURL_GET_H
#define RTORRENT_CORE_CURL_GET_H

#include <iosfwd>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <curl/curl.h>

#include "torrent/utils/scheduler.h"

namespace torrent::net {

class CurlStack;

class CurlGet {
public:
  CurlGet(const std::string& url = "", std::shared_ptr<std::ostream> stream = nullptr);
  ~CurlGet();

  bool                is_stacked() const;
  bool                is_active() const;
  bool                is_using_ipv6() const;

  CurlStack*          curl_stack() const;

  const std::string&  url() const;
  uint32_t            timeout() const;

  int64_t             size_done();
  int64_t             size_total();

  void                reset(const std::string& url, std::shared_ptr<std::ostream> str);

  void                set_timeout(uint32_t seconds);
  void                set_was_started();
  [[nodiscard]] bool  set_was_closed();

  // The owner of the Http object must close it as soon as possible after receiving the signal, as
  // the implementation may allocate limited resources during its lifetime.
  //
  // These slots should not directly modify the CurlGet or CurlStack, and instead use callbacks,
  // etc, for such actions.
  //
  // The slots should be non-blocking and the CurlGet object will remain locked during the call, and
  // as such cannot be modified.
  //
  // The slots won't be called after set_was_closed() is called.

  void                add_done_slot(const std::function<void()>& slot);
  void                add_failed_slot(const std::function<void(const std::string&)>& slot);

  // TODO: Add add_deleted_slot, or a list of threads to remove callbacks from?

protected:
  friend class CurlStack;

  // We need to lock when changing any of the values publically accessible. This means we don't need
  // to lock when changing the underlying vector.
  void                lock() const                    { m_mutex.lock(); }
  auto                lock_guard() const              { return std::scoped_lock(m_mutex); }
  void                unlock() const                  { m_mutex.unlock(); }
  auto&               mutex() const                   { return m_mutex; }

  bool                is_active_unsafe() const        { return m_active; }
  bool                is_closing_unsafe() const       { return m_was_closed; }
  auto                handle_unsafe() const           { return m_handle; }

  [[nodiscard]] bool  prepare_start_unsafe(CurlStack* stack);
  void                activate_unsafe();
  void                cleanup_unsafe();

  // TODO: No locking required?
  void                retry_ipv6();

  void                trigger_done();
  void                trigger_failed(const std::string& message);

private:
  CurlGet(const CurlGet&) = delete;
  CurlGet& operator=(const CurlGet&) = delete;

  static size_t       receive_write(const char* data, size_t size, size_t nmemb, CurlGet* handle);

  mutable std::mutex  m_mutex;

  CURL*               m_handle{};
  CurlStack*          m_stack{};

  bool                m_active{};
  bool                m_was_started{};
  bool                m_was_closed{};
  bool                m_ipv6{};

  // When you change timeout to a different type, update curl_get.cc where it multiplies
  // 1s*m_timeout.

  std::string                   m_url;
  std::shared_ptr<std::ostream> m_stream;
  uint32_t                      m_timeout{5 * 60};

  utils::SchedulerEntry         m_task_timeout;

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

inline uint32_t
CurlGet::timeout() const {
  auto guard = lock_guard();
  return m_timeout;
}

} // namespace torrent::net

#endif

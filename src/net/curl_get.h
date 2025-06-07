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
  CurlGet(CurlStack* s);
  virtual ~CurlGet();

  bool               is_active() const               { return m_active; }
  bool               is_busy() const                 { return m_handle; }
  bool               is_using_ipv6()                 { return m_ipv6; }

  void               start();
  void               close();

  const std::string& url() const                     { return m_url; }
  std::iostream*     stream()                        { return m_stream; }
  uint32_t           timeout() const                 { return m_timeout; }

  // Make sure the output stream does not have any bad/failed bits set.
  void               set_url(const std::string& url) { m_url = url; }
  void               set_stream(std::iostream* str)  { m_stream = str; }
  void               set_timeout(uint32_t seconds)   { m_timeout = seconds; }

  void               retry_ipv6();

  int64_t            size_done();
  int64_t            size_total();

  CURL*              handle()                        { return m_handle; }

  // The owner of the Http object must close it as soon as possible
  // after receiving the signal, as the implementation may allocate
  // limited resources during its lifetime.
  auto&              signal_done()                   { return m_signal_done; }
  auto&              signal_failed()                 { return m_signal_failed; }

private:
  friend class CurlStack;

  CurlGet(const CurlGet&) = delete;
  void operator = (const CurlGet&) = delete;

  // TODO: Atomic?
  void               set_active(bool a)              { m_active = a; }

  void               receive_timeout();

  void               trigger_done();
  void               trigger_failed(const std::string& message);

  bool               m_active{};
  bool               m_ipv6;

  // TODO: Use shared_ptr, or replace with std::function.
  std::string        m_url;
  std::iostream*     m_stream{};

  // When you change this to a different type, update curl_get.cc where it multiplies 1s*m_timeout.
  uint32_t           m_timeout{};

  CURL*              m_handle{};
  CurlStack*         m_stack;

  torrent::utils::SchedulerEntry                     m_task_timeout;
  std::list<std::function<void()>>                   m_signal_done;
  std::list<std::function<void(const std::string&)>> m_signal_failed;
};

}

#endif

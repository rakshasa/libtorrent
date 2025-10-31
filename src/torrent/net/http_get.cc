#include "config.h"

#include "torrent/net/http_get.h"

#include <cassert>
#include <utility>

#include "net/curl_get.h"
#include "net/curl_stack.h"
#include "torrent/exceptions.h"
#include "torrent/net/http_stack.h"
#include "torrent/utils/thread.h"

namespace torrent::net {

// TODO: Use Resolver for dns lookups?

HttpGet::HttpGet() = default;

HttpGet::HttpGet(std::string url, std::shared_ptr<std::ostream> stream)
  : m_curl_get(new CurlGet(std::move(url), std::move(stream))) {
}

HttpGet::~HttpGet() = default;

void
HttpGet::close_and_keep_callbacks() {
  if (!is_valid())
    throw torrent::internal_error("HttpGet::close() called on an invalid HttpGet object.");

  CurlGet::close_and_keep_callbacks(m_curl_get);
}

void
HttpGet::close_and_cancel_callbacks(utils::Thread* callback_thread) {
  if (!is_valid())
    throw torrent::internal_error("HttpGet::close_and_cancel_callbacks() called on an invalid HttpGet object.");

  CurlGet::close_and_cancel_callbacks(m_curl_get, callback_thread);
}

void
HttpGet::wait_for_close() {
  if (!is_valid())
    throw torrent::internal_error("HttpGet::wait_for_close() called on an invalid HttpGet object.");

  m_curl_get->wait_for_close();
}

bool
HttpGet::try_wait_for_close() {
  if (!is_valid())
    return false;

  return m_curl_get->try_wait_for_close();
}

void
HttpGet::reset(const std::string& url, std::shared_ptr<std::ostream> stream) {
  if (m_curl_get == nullptr)
    m_curl_get = std::make_shared<CurlGet>();

  m_curl_get->reset(url, std::move(stream));
}

std::string
HttpGet::url() const {
  return m_curl_get->url();
}

uint32_t
HttpGet::timeout() const {
  return m_curl_get->timeout();
}

int64_t
HttpGet::size_done() const {
  return m_curl_get->size_done();
}

int64_t
HttpGet::size_total() const {
  return m_curl_get->size_total();
}

void
HttpGet::set_timeout(uint32_t seconds) {
  m_curl_get->set_timeout(seconds);
}

void
HttpGet::use_ipv4() {
  m_curl_get->set_initial_resolve(CurlGet::RESOLVE_IPV4);
  m_curl_get->set_retry_resolve(CurlGet::RESOLVE_NONE);
}

void
HttpGet::use_ipv6() {
  m_curl_get->set_initial_resolve(CurlGet::RESOLVE_IPV6);
  m_curl_get->set_retry_resolve(CurlGet::RESOLVE_NONE);
}

void
HttpGet::prefer_ipv4() {
  m_curl_get->set_initial_resolve(CurlGet::RESOLVE_IPV4);
  m_curl_get->set_retry_resolve(CurlGet::RESOLVE_IPV6);
}

void
HttpGet::prefer_ipv6() {
  m_curl_get->set_initial_resolve(CurlGet::RESOLVE_IPV6);
  m_curl_get->set_retry_resolve(CurlGet::RESOLVE_IPV4);
}

void
HttpGet::add_done_slot(const std::function<void()>& slot) {
  if (m_curl_get == nullptr)
    throw torrent::internal_error("HttpGet::add_done_slot() called on an invalid HttpGet object.");

  auto thread = this_thread::thread();

  m_curl_get->add_done_slot([thread, slot, curl_get = m_curl_get.get()]() {
      thread->callback(curl_get, [slot]() {
          slot();
        });
    });
}

void
HttpGet::add_failed_slot(const std::function<void(const std::string&)>& slot) {
  if (m_curl_get == nullptr)
    throw torrent::internal_error("HttpGet::add_failed_slot() called on an invalid HttpGet object.");

  auto thread = this_thread::thread();

  m_curl_get->add_failed_slot([thread, slot, curl_get = m_curl_get.get()](const std::string& error) {
      thread->callback(curl_get, [slot, error]() {
          slot(error);
        });
    });
}

} // namespace torrent::net

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

HttpGet::HttpGet(std::string url, std::shared_ptr<std::ostream> stream) :
    m_curl_get(new CurlGet(std::move(url), std::move(stream))) {
}

HttpGet::~HttpGet() = default;

void
HttpGet::close() {
  if (!is_valid())
    throw torrent::internal_error("HttpGet::close() called on an invalid HttpGet object.");

  auto curl_stack = m_curl_get->curl_stack();

  if (curl_stack == nullptr)
    return;

  if (!m_curl_get->set_was_closed())
    return;

  auto curl_get_weak = std::weak_ptr<CurlGet>(m_curl_get);

  curl_stack->thread()->callback(m_curl_get.get(), [curl_stack, curl_get_weak]() {
      auto curl_get = curl_get_weak.lock();

      if (curl_get)
        curl_stack->close_get(curl_get);
    });
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

void
HttpGet::cancel_slot_callbacks(utils::Thread* thread) {
  if (m_curl_get == nullptr)
    throw torrent::internal_error("HttpGet::cancel_callbacks() called on an invalid HttpGet object.");

  thread->cancel_callback(m_curl_get.get());
}

} // namespace torrent::net

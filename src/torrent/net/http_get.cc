#include "config.h"

#include "torrent/net/http_get.h"

#include <cassert>

#include "net/curl_get.h"
#include "net/curl_stack.h"
#include "net/thread_net.h"
#include "torrent/exceptions.h"
#include "torrent/net/http_stack.h"
#include "torrent/utils/thread.h"

namespace torrent::net {

// TODO: Use Resolver for dns lookups?
// TODO: Start/close should set a bool in curl_get, which indicates we've added it to the callback queue.

HttpGet::HttpGet() = default;

HttpGet::HttpGet(const std::string& url, std::shared_ptr<std::ostream> stream) :
    m_curl_get(std::make_shared<CurlGet>(url, stream)) {
}

void
HttpGet::close() {
  auto curl_stack = m_curl_get->curl_stack();

  // TODO: Callback to thread.

  if (curl_stack != nullptr)
    curl_stack->close_get(m_curl_get);
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
HttpGet::reset(const std::string& url, std::shared_ptr<std::ostream> stream) {
  if (m_curl_get == nullptr)
    m_curl_get = std::make_shared<CurlGet>();

  m_curl_get->reset(url, std::move(stream));
}

void
HttpGet::set_timeout(uint32_t seconds) {
  m_curl_get->set_timeout(seconds);
}

void
HttpGet::add_done_slot(const std::function<void()>& slot) {
  if (m_curl_get == nullptr)
    throw torrent::internal_error("HttpGet::add_done_slot() called on an invalid HttpGet object.");

  m_curl_get->add_done_slot(slot);
}

void
HttpGet::add_failed_slot(const std::function<void(const std::string&)>& slot) {
  if (m_curl_get == nullptr)
    throw torrent::internal_error("HttpGet::add_failed_slot() called on an invalid HttpGet object.");

  m_curl_get->add_failed_slot(slot);
}

} // namespace torrent::net

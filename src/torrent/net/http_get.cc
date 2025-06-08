#include "config.h"

#include "torrent/net/http_get.h"

#include <cassert>

#include "net/curl_get.h"
#include "net/curl_stack.h"
#include "net/thread_net.h"
#include "torrent/exceptions.h"
#include "torrent/utils/thread.h"

namespace torrent::net {

// TODO: Use Resolver for dns lookups?
// TODO: Make thread-safe, lock various parts after start.

// TODO: Start/close should set a bool in curl_get, which indicates we've added it to the callback queue.

HttpGet::HttpGet() = default;

// TODO: Consider the lifetime of stack, should we instead set it on start? Seems more reasonable.
// TODO: Really should pass HttpGet to HttpStack.

HttpGet::HttpGet(CurlStack* stack)
  : m_curl_get(std::make_shared<CurlGet>(stack)) {
}

void
HttpGet::start() {
  m_curl_get->start();
}

void
HttpGet::close() {
  m_curl_get->close();
}

std::string
HttpGet::url() const {
  return m_curl_get->url();
}

std::iostream*
HttpGet::stream() {
  return m_curl_get->stream();
}

uint32_t
HttpGet::timeout() const {
  return m_curl_get->timeout();
}

void
HttpGet::set_url(std::string url) {
  if (m_curl_get->is_active())
    throw torrent::internal_error("Cannot set stream while HttpGet is active.");

  m_curl_get->set_url(std::move(url));
}

void
HttpGet::set_stream(std::iostream* str) {
  if (m_curl_get->is_active())
    throw torrent::internal_error("Cannot set stream while HttpGet is active.");

  m_curl_get->set_stream(str);
}

void
HttpGet::set_timeout(uint32_t seconds) {
  if (m_curl_get->is_active())
    throw torrent::internal_error("Cannot set timeout while HttpGet is active.");

  m_curl_get->set_timeout(seconds);
}

int64_t
HttpGet::size_done() const {
  return m_curl_get->size_done();
}

int64_t
HttpGet::size_total() const {
  return m_curl_get->size_total();
}

// TODO: Wrap in callbacks.

void
HttpGet::add_done_slot(const std::function<void()>& slot) {
  m_curl_get->signal_done().push_back(slot);
}

void
HttpGet::add_failed_slot(const std::function<void(const std::string&)>& slot) {
  m_curl_get->signal_failed().push_back(slot);
}

}

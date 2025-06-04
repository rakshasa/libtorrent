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

HttpGet::HttpGet() = default;

HttpGet::HttpGet(CurlStack* stack)
  : m_curl_get(std::make_shared<CurlGet>(stack)) {
}

void
HttpGet::start() {
  m_curl_get->start();
}

std::string
HttpGet::url() const {
  return m_curl_get->url();
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
HttpGet::add_done_slot(const std::function<void()>& slot) {
  m_curl_get->signal_done().push_back(slot);
}

void
HttpGet::add_failed_slot(const std::function<void(const std::string&)>& slot) {
  m_curl_get->signal_failed().push_back(slot);
}

}

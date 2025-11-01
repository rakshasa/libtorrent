#include "config.h"

#include "curl_stack.h"

#include <algorithm>
#include <cassert>
#include <curl/multi.h>

#include "net/curl_get.h"
#include "net/curl_socket.h"
#include "torrent/exceptions.h"
#include "torrent/utils/thread.h"

namespace torrent::net {

CurlStack::CurlStack(utils::Thread* thread)
  : m_thread(thread),
    m_handle(curl_multi_init()) {

  m_task_timeout.slot() = [this]() { receive_timeout(); };

  curl_multi_setopt(m_handle, CURLMOPT_TIMERDATA, this);
  curl_multi_setopt(m_handle, CURLMOPT_TIMERFUNCTION, &CurlStack::set_timeout);
  curl_multi_setopt(m_handle, CURLMOPT_SOCKETDATA, this);
  curl_multi_setopt(m_handle, CURLMOPT_SOCKETFUNCTION, &CurlSocket::receive_socket);

  curl_multi_setopt(m_handle, CURLMOPT_MAXCONNECTS, m_max_cache_connections);
  curl_multi_setopt(m_handle, CURLMOPT_MAX_HOST_CONNECTIONS, m_max_host_connections);
  curl_multi_setopt(m_handle, CURLMOPT_MAX_TOTAL_CONNECTIONS, m_max_total_connections);
}

CurlStack::~CurlStack() {
  assert(!is_running() && "CurlStack::~CurlStack() called while still running.");
}

void
CurlStack::set_max_cache_connections(unsigned int value) {
  if (value > 1024)
    throw torrent::internal_error("CurlStack::set_max_cache_connections() called with a value greater than 1024.");

  auto guard = lock_guard();

  m_max_cache_connections = value;

  if (curl_multi_setopt(m_handle, CURLMOPT_MAXCONNECTS, value) != CURLM_OK)
    throw torrent::internal_error("CurlStack::set_max_cache_connections() error calling curl_multi_setopt.");
}

void
CurlStack::set_max_host_connections(unsigned int value) {
  if (value > 1024)
    throw torrent::internal_error("CurlStack::set_max_host_connections() called with a value greater than 1024.");

  auto guard = lock_guard();

  m_max_host_connections = value;

  if (curl_multi_setopt(m_handle, CURLMOPT_MAX_HOST_CONNECTIONS, value) != CURLM_OK)
    throw torrent::internal_error("CurlStack::set_max_host_connections() error calling curl_multi_setopt.");
}

void
CurlStack::set_max_total_connections(unsigned int value) {
  if (value > 4096)
    throw torrent::internal_error("CurlStack::set_max_total_connections() called with a value greater than 4096.");

  auto guard = lock_guard();

  m_max_total_connections = value;

  if (curl_multi_setopt(m_handle, CURLMOPT_MAX_TOTAL_CONNECTIONS, value) != CURLM_OK)
    throw torrent::internal_error("CurlStack::set_max_total_connections() error calling curl_multi_setopt.");
}

void
CurlStack::shutdown() {
  assert(std::this_thread::get_id() == m_thread->thread_id());

  { auto guard = lock_guard();

    assert(m_running && "CurlStack::shutdown() called while not running.");
    m_running = false;
  }

  while (!base_type::empty())
    close_get(base_type::back());

  curl_multi_cleanup(m_handle);

  m_handle = nullptr;
  torrent::this_thread::scheduler()->erase(&m_task_timeout);
}

void
CurlStack::start_get(const std::shared_ptr<CurlGet>& curl_get) {
  assert(std::this_thread::get_id() == m_thread->thread_id());

  if (curl_get == nullptr)
    throw torrent::internal_error("CurlStack::start_get() called with a null curl_get.");

  { auto guard = std::scoped_lock(m_mutex, curl_get->mutex());

    // TODO: Check is_running, if not return error. Do not throw internal_error.
    if (!m_running)
      throw torrent::internal_error("CurlStack::start_get() called while not running.");

    // TODO: This might cause a race condition on cleanup_unsafe while not active?
    if (!curl_get->prepare_start_unsafe(this))
      return; // CurlGet was already closed.

    if (!m_user_agent.empty())
      curl_easy_setopt(curl_get->handle_unsafe(), CURLOPT_USERAGENT, m_user_agent.c_str());

    if (!m_http_proxy.empty())
      curl_easy_setopt(curl_get->handle_unsafe(), CURLOPT_PROXY, m_http_proxy.c_str());

    if (!m_http_ca_path.empty())
      curl_easy_setopt(curl_get->handle_unsafe(), CURLOPT_CAPATH, m_http_ca_path.c_str());

    if (!m_http_ca_cert.empty())
      curl_easy_setopt(curl_get->handle_unsafe(), CURLOPT_CAINFO, m_http_ca_cert.c_str());

    curl_easy_setopt(curl_get->handle_unsafe(), CURLOPT_SSL_VERIFYHOST, m_ssl_verify_host ? 2l : 0l);
    curl_easy_setopt(curl_get->handle_unsafe(), CURLOPT_SSL_VERIFYPEER, m_ssl_verify_peer ? 1l : 0l);
    curl_easy_setopt(curl_get->handle_unsafe(), CURLOPT_DNS_CACHE_TIMEOUT, m_dns_timeout);

    base_type::push_back(curl_get);

    curl_get->activate_unsafe();
  }
}

void
CurlStack::close_get(const std::shared_ptr<CurlGet>& curl_get) {
  assert(std::this_thread::get_id() == m_thread->thread_id());

  { auto guard_get = curl_get->lock_guard();

    if (!curl_get->is_prepare_canceled_unsafe()) {
      auto itr = std::find(base_type::begin(), base_type::end(), curl_get);

      if (itr == base_type::end())
        throw torrent::internal_error("CurlStack::close_get() called on a CurlGet that is not in the stack.");

      base_type::erase(itr);
    }

    curl_get->cleanup_unsafe();
  }

  curl_get->notify_closed();
}

CurlStack::base_type::iterator
CurlStack::find_curl_handle(const CURL* curl_handle) {
  auto itr = std::find_if(base_type::begin(), base_type::end(), [curl_handle](auto& curl_get) {
    return curl_get->handle_unsafe() == curl_handle;
  });

  if (itr == base_type::end())
    throw torrent::internal_error("Could not find CurlGet with the right easy_handle.");

  return itr;
}

int
CurlStack::set_timeout(void*, long timeout_ms, CurlStack* stack) {
  assert(std::this_thread::get_id() == stack->m_thread->thread_id());

  if (timeout_ms == -1)
    torrent::this_thread::scheduler()->erase(&stack->m_task_timeout);
  else
    torrent::this_thread::scheduler()->update_wait_for_ceil_seconds(&stack->m_task_timeout, std::chrono::milliseconds(timeout_ms));

  return 0;
}

void
CurlStack::receive_timeout() {
  assert(std::this_thread::get_id() == m_thread->thread_id());

  int count{};
  auto code = curl_multi_socket_action(m_handle, CURL_SOCKET_TIMEOUT, 0, &count);

  if (code != CURLM_OK)
    throw torrent::internal_error("CurlStack::receive_timeout() error calling curl_multi_socket_action.");

  while (process_done_handle())
    ; // Do nothing.

  if (base_type::empty()) {
    torrent::this_thread::scheduler()->erase(&m_task_timeout);
    return;
  }

  if (!m_task_timeout.is_scheduled()) {
    // Sometimes libcurl forgets to reset the timeout. Try to poll the value in that case, or use 10
    // seconds max.
    long timeout_ms;
    curl_multi_timeout(m_handle, &timeout_ms);

    auto timeout = std::max<std::chrono::microseconds>(std::chrono::milliseconds(timeout_ms), 10s);

    torrent::this_thread::scheduler()->wait_for_ceil_seconds(&m_task_timeout, timeout);
  }
}

// TODO: Check if curl_get is still active.

bool
CurlStack::process_done_handle() {
  int remaining_msgs{};

  CURLMsg* msg = curl_multi_info_read(m_handle, &remaining_msgs);

  if (msg == nullptr)
    return false;

  if (msg->msg != CURLMSG_DONE)
    throw torrent::internal_error("CurlStack::process_done_handle() msg->msg != CURLMSG_DONE.");

  // TODO: Search if handle is still in the stack, and if not, assume it was closed and clean up. (check is_active)

  auto itr = find_curl_handle(msg->easy_handle);

  // TODO: Lock CurlGet here, do the retry or retrieve the slots, instead of using trigger_*.

  // Strictly not needed as the following conditions will also return false if the handle is
  // closing.
  if ((*itr)->is_closing())
    return remaining_msgs != 0;

  if (msg->data.result == CURLE_COULDNT_RESOLVE_HOST && (*itr)->retry_resolve())
    return remaining_msgs != 0;

  if (msg->data.result == CURLE_OK)
    (*itr)->trigger_done();
  else
    (*itr)->trigger_failed(curl_easy_strerror(msg->data.result));

  // TODO: Should start next download here rather than in close_get.

  return remaining_msgs != 0;
}

} // namespace torrent::net

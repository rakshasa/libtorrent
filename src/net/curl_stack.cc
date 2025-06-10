#include "config.h"

#include "curl_stack.h"

#include <algorithm>
#include <cassert>
#include <curl/multi.h>

#include "net/curl_get.h"
#include "net/curl_socket.h"
#include "torrent/exceptions.h"

namespace torrent::net {

CurlStack::CurlStack() {
  m_handle = curl_multi_init();
  m_task_timeout.slot() = [this]() { receive_timeout(); };

  curl_multi_setopt(m_handle, CURLMOPT_TIMERDATA, this);
  curl_multi_setopt(m_handle, CURLMOPT_TIMERFUNCTION, &CurlStack::set_timeout);
  curl_multi_setopt(m_handle, CURLMOPT_SOCKETDATA, this);
  curl_multi_setopt(m_handle, CURLMOPT_SOCKETFUNCTION, &CurlSocket::receive_socket);
}

CurlStack::~CurlStack() {
  assert(!m_running && "CurlStack::~CurlStack() called while still running.");
}

// TODO: We do not ever call shutdown.
void
CurlStack::shutdown() {
  {
    auto guard = lock_guard();

    if (!m_running)
      return;

    // TODO: This needs to be done after all CurlGet objects have been closed and cleaned up, as
    // CurlSocket check is_running.
    m_running = false;
  }

  while (!base_type::empty())
    close_get(base_type::back());

  curl_multi_cleanup(m_handle);

  torrent::this_thread::scheduler()->erase(&m_task_timeout);
}

void
CurlStack::start_get(std::shared_ptr<CurlGet> curl_get) {
  // TODO: When this is made into a callback, add a bool to indicate that we have queued the
  // callbacks for start/close.

  // TODO: Check is_running?

  {
    auto guard = lock_guard();

    curl_get->prepare_start(this);

    if (!m_user_agent.empty())
      curl_easy_setopt(curl_get->handle(), CURLOPT_USERAGENT, m_user_agent.c_str());

    if (!m_http_proxy.empty())
      curl_easy_setopt(curl_get->handle(), CURLOPT_PROXY, m_http_proxy.c_str());

    if (!m_bind_address.empty())
      curl_easy_setopt(curl_get->handle(), CURLOPT_INTERFACE, m_bind_address.c_str());

    if (!m_http_ca_path.empty())
      curl_easy_setopt(curl_get->handle(), CURLOPT_CAPATH, m_http_ca_path.c_str());

    if (!m_http_ca_cert.empty())
      curl_easy_setopt(curl_get->handle(), CURLOPT_CAINFO, m_http_ca_cert.c_str());

    curl_easy_setopt(curl_get->handle(), CURLOPT_SSL_VERIFYHOST, m_ssl_verify_host ? 2l : 0l);
    curl_easy_setopt(curl_get->handle(), CURLOPT_SSL_VERIFYPEER, m_ssl_verify_peer ? 1l : 0l);
    curl_easy_setopt(curl_get->handle(), CURLOPT_DNS_CACHE_TIMEOUT, m_dns_timeout);

    base_type::push_back(curl_get);

    if (m_active >= m_max_active)
      return;

    m_active++;

    curl_get->activate();
  }
}

// TODO: We are currently requiring close to be called before activating the next download. This
// should happen after transfer_done.

void
CurlStack::close_get(std::shared_ptr<CurlGet> curl_get) {
  if (!curl_get->is_active())
    throw torrent::internal_error("Tried to close CurlGet that is not active.");

  auto itr = std::find(base_type::begin(), base_type::end(), curl_get);

  if (itr == base_type::end())
    throw torrent::internal_error("Could not find CurlGet in CurlStack::remove_get().");

  base_type::erase(itr);
  curl_get->cleanup();

  {
    auto guard = lock_guard();

    if (m_active > m_max_active) {
      m_active--;
      return;
    }

    itr = std::find_if(base_type::begin(), base_type::end(), [](auto& curl_get) {
        return !curl_get->is_active();
      });

    if (itr == base_type::end()) {
      m_active--;
      return;
    }

    (*itr)->activate();
  }
}

CurlStack::base_type::iterator
CurlStack::find_curl_handle(const CURL* curl_handle) {
  auto itr = std::find_if(base_type::begin(), base_type::end(), [curl_handle](const std::shared_ptr<CurlGet>& curl_get) {
    return curl_get->handle() == curl_handle;
  });

  if (itr == base_type::end())
    throw torrent::internal_error("Could not find CurlGet with the right easy_handle.");

  return itr;
}

int
CurlStack::set_timeout(void*, long timeout_ms, void* userp) {
  CurlStack* stack = static_cast<CurlStack*>(userp);

  if (timeout_ms == -1)
    torrent::this_thread::scheduler()->erase(&stack->m_task_timeout);
  else
    torrent::this_thread::scheduler()->update_wait_for_ceil_seconds(&stack->m_task_timeout, std::chrono::milliseconds(timeout_ms));

  return 0;
}

void
CurlStack::receive_timeout() {
  int count{};
  auto code = curl_multi_socket_action(m_handle, CURL_SOCKET_TIMEOUT, 0, &count);

  if (code != CURLM_OK)
    throw torrent::internal_error("CurlStack::receive_timeout() error calling curl_multi_socket_action.");

  while (process_done_handle())
    ; // Do nothing.

  if (!empty() && !m_task_timeout.is_scheduled()) {
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

  if (msg->data.result == CURLE_COULDNT_RESOLVE_HOST) {
    auto itr = find_curl_handle(static_cast<CURL*>(msg->easy_handle));

    if (!(*itr)->is_using_ipv6()) {
      (*itr)->retry_ipv6();

      if (curl_multi_add_handle(m_handle, (*itr)->handle()) > 0)
        throw torrent::internal_error("Error calling curl_multi_add_handle.");

      return remaining_msgs != 0;
    }
  }

  if (msg->data.result == CURLE_OK)
    process_transfer_done(msg->easy_handle, nullptr);
  else
    process_transfer_done(msg->easy_handle, curl_easy_strerror(msg->data.result));

  if (base_type::empty())
    torrent::this_thread::scheduler()->erase(&m_task_timeout);

  return remaining_msgs != 0;
}

void
CurlStack::process_transfer_done(CURL* handle, const char* msg) {
  auto itr = find_curl_handle(handle);

  // TODO: Lock-guard.

  // Don't use trigger_* here...
  if (msg == nullptr)
    (*itr)->trigger_done();
  else
    (*itr)->trigger_failed(msg);
}

} // namespace torrent::net

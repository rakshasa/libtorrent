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
  m_handle = (void*)curl_multi_init();
  m_task_timeout.slot() = [this]() { receive_timeout(); };

  curl_multi_setopt((CURLM*)m_handle, CURLMOPT_TIMERDATA, this);
  curl_multi_setopt((CURLM*)m_handle, CURLMOPT_TIMERFUNCTION, &CurlStack::set_timeout);
  curl_multi_setopt((CURLM*)m_handle, CURLMOPT_SOCKETDATA, this);
  curl_multi_setopt((CURLM*)m_handle, CURLMOPT_SOCKETFUNCTION, &CurlSocket::receive_socket);
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

    m_running = false;
  }

  while (true) {
    // TODO: This is wrong, we need to have a close that doesn't call remove_get.

    auto curl_get = [this]() -> CurlGet* {
        auto guard = lock_guard();

        if (base_type::empty())
          return nullptr;

        return base_type::back();
      }();

    if (curl_get == nullptr)
      break;

    curl_get->close();
  }

  curl_multi_cleanup((CURLM*)m_handle);

  torrent::this_thread::scheduler()->erase(&m_task_timeout);
}

void
CurlStack::receive_action(CurlSocket* socket, int events) {
  CURLMcode code;

  do {
    int count;

    code = curl_multi_socket_action((CURLM*)m_handle,
                                    socket != nullptr ? socket->file_descriptor() : CURL_SOCKET_TIMEOUT,
                                    events,
                                    &count);

    if (code > 0)
      throw torrent::internal_error("Error calling curl_multi_socket_action.");

    // Socket might be removed when cleaning handles below, future
    // calls should not use it.
    socket = nullptr;
    events = 0;

    while (process_done_handle())
      ; // Do nothing.

  } while (code == CURLM_CALL_MULTI_PERFORM);
}

bool
CurlStack::process_done_handle() {
  int remaining_msgs = 0;

  CURLMsg* msg = curl_multi_info_read((CURLM*)m_handle, &remaining_msgs);

  if (msg == nullptr)
    return false;

  if (msg->msg != CURLMSG_DONE)
    throw torrent::internal_error("CurlStack::process_done_handle() msg->msg != CURLMSG_DONE.");

  if (msg->data.result == CURLE_COULDNT_RESOLVE_HOST) {
    auto itr = find_curl_get((CurlGet*)msg->easy_handle);

    if (!(*itr)->is_using_ipv6()) {
      (*itr)->retry_ipv6();

      if (curl_multi_add_handle((CURLM*)m_handle, (*itr)->handle()) > 0)
        throw torrent::internal_error("Error calling curl_multi_add_handle.");

      return remaining_msgs != 0;
    }
  }

  transfer_done(msg->easy_handle,
                msg->data.result == CURLE_OK ? nullptr : curl_easy_strerror(msg->data.result));

  if (base_type::empty())
    torrent::this_thread::scheduler()->erase(&m_task_timeout);

  return remaining_msgs != 0;
}

// Lock before calling.
void
CurlStack::transfer_done(void* handle, const char* msg) {
  auto itr = find_curl_get((CurlGet*)handle);

  if (msg == nullptr)
    (*itr)->trigger_done();
  else
    (*itr)->trigger_failed(msg);
}

// TODO: Is this function supposed to set a per-handle timeout, or is
// it the shortest timeout amongst all handles?
int
CurlStack::set_timeout(void*, long timeout_ms, void* userp) {
  CurlStack* stack = (CurlStack*)userp;

  torrent::this_thread::scheduler()->update_wait_for_ceil_seconds(&stack->m_task_timeout, std::chrono::milliseconds(timeout_ms));
  return 0;
}

void
CurlStack::receive_timeout() {
  receive_action(nullptr, 0);

  if (!empty() && !m_task_timeout.is_scheduled()) {
    // Sometimes libcurl forgets to reset the timeout. Try to poll the value in that case, or use 10
    // seconds max.
    long timeout_ms;
    curl_multi_timeout((CURLM*)m_handle, &timeout_ms);

    auto timeout = std::max<std::chrono::microseconds>(std::chrono::milliseconds(timeout_ms), 10s);

    torrent::this_thread::scheduler()->wait_for_ceil_seconds(&m_task_timeout, timeout);
  }
}

// TODO: Change to use std::shared_ptr, however only store the pointer.

void
CurlStack::add_get(CurlGet* curl_get) {
  {
    auto guard = lock_guard();

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

    curl_easy_setopt(curl_get->handle(), CURLOPT_SSL_VERIFYHOST, (long)(m_ssl_verify_host ? 2 : 0));
    curl_easy_setopt(curl_get->handle(), CURLOPT_SSL_VERIFYPEER, (long)(m_ssl_verify_peer ? 1 : 0));
    curl_easy_setopt(curl_get->handle(), CURLOPT_DNS_CACHE_TIMEOUT, m_dns_timeout);

    base_type::push_back(curl_get);

    if (m_active >= m_max_active)
      return;

    m_active++;
  }

  // TODO: Lock get.

  curl_get->set_active(true);

  if (curl_multi_add_handle((CURLM*)m_handle, curl_get->handle()) > 0)
    throw torrent::internal_error("Error calling curl_multi_add_handle.");
}

void
CurlStack::remove_get(CurlGet* curl_get) {
  {
    auto guard = lock_guard();

    base_type::erase(find_curl_get(curl_get));
  }

  // The CurlGet object was never activated, so we just skip this one.
  if (!curl_get->is_active())
    return;

  curl_get->set_active(false);

  if (curl_multi_remove_handle((CURLM*)m_handle, curl_get->handle()) > 0)
    throw torrent::internal_error("Error calling curl_multi_remove_handle.");

  {
    auto guard = lock_guard();

    if (m_active != m_max_active) {
      m_active--;
      return;
    }

    auto itr = std::find_if(base_type::begin(), base_type::end(), [](CurlGet* curl_get) { return !curl_get->is_active(); });

    if (itr == base_type::end())
      return;

    (*itr)->set_active(true);

    if (curl_multi_add_handle((CURLM*)m_handle, (*itr)->handle()) > 0)
      throw torrent::internal_error("Error calling curl_multi_add_handle.");
  }
}

CurlStack::base_type::iterator
CurlStack::find_curl_get(CurlGet* curl_get) {
  auto itr = std::find_if(base_type::begin(), base_type::end(), [curl_get](auto c) { return c->handle() == curl_get; });

  if (itr == base_type::end())
    throw torrent::internal_error("Could not find CurlGet with the right easy_handle.");

  return itr;
}

} // namespace torrent::net

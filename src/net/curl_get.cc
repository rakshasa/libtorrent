#include "config.h"

#include "net/curl_get.h"

#include <iostream>
#include <curl/easy.h>

#include "net/curl_stack.h"
#include "torrent/exceptions.h"
#include "utils/functional.h"

namespace torrent::net {

static size_t
curl_get_receive_write(void* data, size_t size, size_t nmemb, void* handle) {
  if (!((CurlGet*)handle)->stream()->write((const char*)data, size * nmemb).fail())
    return size * nmemb;
  else
    return 0;
}

CurlGet::CurlGet(CurlStack* s)
  : m_stack(s) {

  m_task_timeout.slot() = [this]() { receive_timeout(); };
}

CurlGet::~CurlGet() {
  close();
}

void
CurlGet::start() {
  if (is_busy())
    throw torrent::internal_error("Tried to call CurlGet::start on a busy object.");

  if (m_stream == NULL)
    throw torrent::internal_error("Tried to call CurlGet::start without a valid output stream.");

  if (!m_stack->is_running())
    return;

  m_handle = curl_easy_init();

  if (m_handle == NULL)
    throw torrent::internal_error("Call to curl_easy_init() failed.");

  curl_easy_setopt(m_handle, CURLOPT_URL,            m_url.c_str());
  curl_easy_setopt(m_handle, CURLOPT_WRITEFUNCTION,  &curl_get_receive_write);
  curl_easy_setopt(m_handle, CURLOPT_WRITEDATA,      this);

  if (m_timeout != 0) {
    curl_easy_setopt(m_handle, CURLOPT_CONNECTTIMEOUT, (long)60);
    curl_easy_setopt(m_handle, CURLOPT_TIMEOUT,        (long)m_timeout);

    // Normally libcurl should handle the timeout. But sometimes that doesn't
    // work right so we do a fallback timeout that just aborts the transfer.
    torrent::this_thread::scheduler()->update_wait_for_ceil_seconds(&m_task_timeout, 5s + 1s*m_timeout);
  }

  curl_easy_setopt(m_handle, CURLOPT_FORBID_REUSE,   (long)1);
  curl_easy_setopt(m_handle, CURLOPT_NOSIGNAL,       (long)1);
  curl_easy_setopt(m_handle, CURLOPT_FOLLOWLOCATION, (long)1);
  curl_easy_setopt(m_handle, CURLOPT_MAXREDIRS,      (long)5);

  curl_easy_setopt(m_handle, CURLOPT_IPRESOLVE,      CURL_IPRESOLVE_WHATEVER);

  curl_easy_setopt(m_handle, CURLOPT_ENCODING,       "");

  m_ipv6 = false;

  m_stack->add_get(this);
}

void
CurlGet::close() {
  torrent::this_thread::scheduler()->erase(&m_task_timeout);

  if (!is_busy())
    return;

  m_stack->remove_get(this);

  curl_easy_cleanup(m_handle);
  m_handle = NULL;
}

void
CurlGet::retry_ipv6() {
  CURL* nhandle = curl_easy_duphandle(m_handle);

  curl_easy_setopt(nhandle, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V6);
  curl_easy_cleanup(m_handle);

  m_handle = nhandle;
  m_ipv6 = true;
}

int64_t
CurlGet::size_done() {
  curl_off_t d = 0;
  curl_easy_getinfo(m_handle, CURLINFO_SIZE_DOWNLOAD_T, &d);

  return d;
}

int64_t
CurlGet::size_total() {
  curl_off_t d = 0;
  curl_easy_getinfo(m_handle, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &d);

  return d;
}

void
CurlGet::receive_timeout() {
  return m_stack->transfer_done(m_handle, "Timed out");
}

void
CurlGet::trigger_done() {
  ::utils::slot_list_call(m_signal_done);

  // if (should_delete_stream) {
  //   delete m_stream;
  //   m_stream = NULL;
  // }
}

void
CurlGet::trigger_failed(const std::string& message) {
  ::utils::slot_list_call(m_signal_failed, message);

  // if (should_delete_stream) {
  //   delete m_stream;
  //   m_stream = NULL;
  // }
}

}

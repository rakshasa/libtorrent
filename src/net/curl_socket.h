#ifndef RTORRENT_CORE_CURL_SOCKET_H
#define RTORRENT_CORE_CURL_SOCKET_H

#include <curl/curl.h>

#include "torrent/event.h"

namespace torrent::net {

class CurlStack;

class CurlSocket : public torrent::Event {
public:
  CurlSocket(int fd, CurlStack* stack) : m_stack(stack) { m_fileDesc = fd; }
  ~CurlSocket() override;

  const char*        type_name() const override { return "curl_socket"; }

  void               close();

  void               event_read() override;
  void               event_write() override;
  void               event_error() override;

  static int         receive_socket(void* easy_handle, curl_socket_t fd, int what, CurlStack* userp, CurlSocket* socketp);

private:
  CurlSocket(const CurlSocket&) = delete;
  CurlSocket& operator=(const CurlSocket&) = delete;

  void               handle_action(int ev_bitmask);

  CurlStack*         m_stack{};
};

} // namespace torrent::net

#endif

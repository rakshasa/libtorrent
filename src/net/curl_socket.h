#ifndef LIBTORRENT_NET_CURL_SOCKET_H
#define LIBTORRENT_NET_CURL_SOCKET_H

#include <curl/curl.h>

#include "net/curl_stack.h"
#include "torrent/event.h"

namespace torrent::net {

class CurlGet;

class CurlSocket : public torrent::Event {
public:
  // TODO: Deprecate.
  CurlSocket(int fd, CurlStack* stack, CURL* easy_handle);

  CurlSocket(CurlStack* stack);
  ~CurlSocket() override;

  const char*         type_name() const override { return "curl_socket"; }

  void                event_read() override;
  void                event_write() override;
  void                event_error() override;

protected:
  friend class CurlGet;
  friend class CurlStack;

  static int           receive_socket(CURL* easy_handle, curl_socket_t fd, int what, CurlStack* userp, CurlSocket* socketp);
  static curl_socket_t open_socket(CurlStack *stack, curlsocktype purpose, struct curl_sockaddr *address);
  static int           close_socket(CurlStack* stack, curl_socket_t fd);

private:
  CurlSocket(const CurlSocket&) = delete;
  CurlSocket& operator=(const CurlSocket&) = delete;

  void                handle_action(int fd, int ev_bitmask);

  void                clear_and_erase_self(CurlStack::socket_map_type::iterator itr);
  void                clear_and_erase_self_or_throw();

  CurlStack*          m_stack{};
  CURL*               m_easy_handle{};

  bool                m_self_exists{true};
  bool                m_properly_opened{false};
  bool                m_uninterested{};
};

} // namespace torrent::net

#endif // LIBTORRENT_NET_CURL_SOCKET_H

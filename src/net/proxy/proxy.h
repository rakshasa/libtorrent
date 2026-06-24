#ifndef LIBTORRENT_NET_PROXY_PROXY_H
#define LIBTORRENT_NET_PROXY_PROXY_H

#include <torrent/common.h>

namespace torrent::net::proxy {

class Proxy {
public:
  static constexpr int state_reading   = 1;
  static constexpr int state_writing   = 2;
  static constexpr int state_error     = 3;
  static constexpr int state_finished  = 4;

  virtual ~Proxy() = default;

  virtual int         next_action() = 0;

  // Returns the number of bytes consumed, or 0 if no bytes were consumed.
  virtual uint32_t    read(const char* data, uint32_t size) = 0;
  virtual uint32_t    write(char* data, uint32_t max_size) = 0;

private:
};

} // namespace torrent::net::proxy

#endif
